/*
 * Copyright 2016 Hewlett Packard Enterprise Development LP.
 *
 * This program is free software; you can redistribute it and.or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version. This program is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details. You
 * should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/**
 *  \file
 * 
 *  Constructs a virtual topology
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <numa.h>
#include <libxml/encoding.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>


#include "cpu/cpu.h"
#include "error.h"
#include "graph.h"
#include "measure.h"
#include "misc.h"
#include "topology.h"
#include "model.h"
#include "throttle.h"

#define MAX_NUM_MC_PCI_BUS 16

#define MY_ENCODING "ISO-8859-1"

extern latency_model_t latency_model;

int num_cpus(struct bitmask* bitmask) 
{
    int i,n;
    // if we had knowledge of the bitmask structure then we could
    // count the bits faster but bitmask seems to be an opaque structure
    for (i=0, n=0; i<numa_num_configured_cpus(); i++) {
        if (numa_bitmask_isbitset(bitmask, i)) {
            n++;
        }
    }
    return n;
}

// number of cpus in the system
int system_num_cpus()
{
    return sysconf( _SC_NPROCESSORS_ONLN );
}

void print_bitmask(struct bitmask* bitmask) {
    int i;
    for (i=0; i<numa_num_configured_cpus(); i++) {
        if (numa_bitmask_isbitset(bitmask, i)) {
            DBG_LOG(INFO, "bit %d\n", i);
        }
    }
    return;
}

int next_cpu(struct bitmask* bitmask, int cpu_id)
{
    int i;
    // if we had knowledge of the bitmask structure then we could
    // count the bits faster but bitmask seems to be an opaque structure
    for (i=cpu_id; i<numa_num_configured_cpus(); i++) {
        if (numa_bitmask_isbitset(bitmask, i)) {
            return i;
        }
    }
    return -1;
}

int first_cpu(struct bitmask* bitmask)
{
    return next_cpu( bitmask, 0);
}

/**
 * \brief Translate virtual cpu node id to physical cpu id
 *
 * \todo Make the translation more efficient by keeping an array map.
 */
int virtual_cpu_id_to_phys_cpu_id(virtual_node_t* vnode, int cpu_id)
{
    int count;
    int i;
    struct bitmask* bitmask = vnode->dram_node->cpu_bitmask;

    for (i=0, count=0; i<numa_num_configured_cpus(); i++) {
        if (numa_bitmask_isbitset(bitmask, i)) {
            if (count == cpu_id) {
                return i;
            }
            count++;
        }
    }
    return -1;
}

int memory_controller_id(char* str)
{
    int memctr_id;
    char* memctr = strstr(str, "Memory Controller");
    if (!memctr) return -1;
    if (sscanf(memctr, "Memory Controller %d", &memctr_id) != 1) return -1;
    return memctr_id;
}

/** 
 *  \brief Returns a list of memory-controller pci buses
 */
int get_mc_pci_bus_list(pci_regs_t *bus_id_list[], int max_list_size, int* dev_countp)
{
    FILE* fp;
    char  buf[2048];
    int   bus_id, dev_id, funct;
    int   last_bus_id = -1;
    int   channel = 0;
    char  dontcare[512];
    int   dev_count = 0;

    fp = popen("lspci", "r");
    if (fp == NULL) {
        return E_ERROR;
    }

    for (dev_count=0; fgets(buf, sizeof(buf)-1, fp) != NULL; ) {
        if (strstr(buf, "Thermal Control")) {
            if (sscanf(buf, "%x:%x.%x %s", &bus_id, &dev_id, &funct, dontcare) == 4) {
                if (bus_id != last_bus_id) {
                    ++dev_count;
                    last_bus_id = bus_id;

                    if (dev_count > max_list_size) {
                        pclose(fp);
                        return E_ERROR;
                    }
                    channel = 0;
                    bus_id_list[dev_count-1] = (pci_regs_t*)malloc(sizeof(pci_regs_t));
                }

                bus_id_list[dev_count-1]->addr[channel].bus_id = bus_id;
                bus_id_list[dev_count-1]->addr[channel].dev_id = dev_id;
                bus_id_list[dev_count-1]->addr[channel].funct = funct;
                ++channel;
                bus_id_list[dev_count-1]->channels = channel;
            }
        }
    }
    *dev_countp = dev_count;
    pclose(fp);

    return E_SUCCESS;
}

/**
 *  \brief Finds out the physical memory-controller pci bus topology of the 
 *  machine, which includes the socket each memory controller is attached to
 * 
 *  To find where a memory controller is connected to, we throttle the rest of 
 *  the memory controllers and measure local bandwidth of each node. The unthrottled 
 *  memory controller is attached to the node with the highest local bandwidth
 */
int discover_mc_pci_topology(cpu_model_t* cpu_model, physical_topology_t* physical_topology)
{
    pci_regs_t *regs_addr[16];
    int dev_count;
    physical_node_t* local_node = NULL;
    int b, i;
    double max_local_rbw;
    double rbw;
    int count = 0;
    uint16_t throttle_reg_val;

    get_mc_pci_bus_list(regs_addr, MAX_NUM_MC_PCI_BUS, &dev_count);

    if (dev_count < physical_topology->num_nodes) {
        // TODO: application is terminated on error only if in DEBUG mode
        DBG_LOG(WARNING, "The number of physical nodes is greater than the number of memory-controller pci buses.\n");
    }

    for (b=0; b<dev_count; b++) {
        // throttle all other buses except the one we are currently trying 
        // to figure out where it is attached
        for (i=0; i<dev_count; i++) {
            if (i == b) {
                //cpu_model->get_throttle_register(regs_addr[i], THROTTLE_DDR_ACT, &throttle_reg_val);
                //if (throttle_reg_val < 0x8fff)
                cpu_model->set_throttle_register(regs_addr[i], THROTTLE_DDR_ACT, 0x8fff);
            } else {
                cpu_model->set_throttle_register(regs_addr[i], THROTTLE_DDR_ACT, 0x800f);
            }
        }
        
        // measure local bandwidth of each node
        max_local_rbw = 0;
        local_node = NULL;
        for (i=0; i<physical_topology->num_nodes; i++) {
            physical_node_t* node_i = &physical_topology->physical_nodes[i];
            rbw = measure_read_bw(node_i->node_id, node_i->node_id);
            DBG_LOG(DEBUG, "bandwidth cpu node %d mem node %d: bw %lu\n", node_i->node_id, node_i->node_id, (unsigned int) rbw);
            if (rbw > max_local_rbw) {
                max_local_rbw = rbw;
                local_node = node_i;
            }
        }
        if (local_node) {
            DBG_LOG(DEBUG, "setting node_id %d to bus %X\n", local_node->node_id, regs_addr[b]->addr[0].bus_id);
            local_node->mc_pci_regs = regs_addr[b];
            if (++count == physical_topology->num_nodes) break;
        }
    }

    for (i=0; i<dev_count; i++) {
        cpu_model->get_throttle_register(regs_addr[i], THROTTLE_DDR_ACT, &throttle_reg_val);
        if (throttle_reg_val < 0x8fff)
            cpu_model->set_throttle_register(regs_addr[i], THROTTLE_DDR_ACT, 0x8fff);
    }

    return E_SUCCESS;
}

/**
 *  \brief Finds out the physical topology of the machine, including memory 
 *  latency, memory bandwidth, and pci bus information.
 */
int discover_physical_topology(cpu_model_t* cpu_model, physical_topology_t** physical_topology)
{
    int i, j, n;
    int rc;
    physical_topology_t* pt;

    if (!(pt = malloc(sizeof(**physical_topology)))) {
        rc = E_NOMEM;
        goto err;
    }
    
    pt->num_nodes = numa_num_configured_nodes();
    if (!(pt->physical_nodes = calloc(pt->num_nodes, sizeof(*pt->physical_nodes)))) {
        DBG_LOG(ERROR, "Failed physical nodes allocation\n");
        abort();
    }

    for (n=0; n<pt->num_nodes; n++) {
        memset(&pt->physical_nodes[n], 0, sizeof(*pt->physical_nodes));
        pt->physical_nodes[n].node_id = n;
        pt->physical_nodes[n].cpu_bitmask = numa_allocate_cpumask();
        pt->physical_nodes[n].cpu_model = cpu_model;
        numa_node_to_cpus(n, pt->physical_nodes[n].cpu_bitmask);
        pt->physical_nodes[n].num_cpus = num_cpus(pt->physical_nodes[n].cpu_bitmask);
        pt->physical_nodes[n].latencies = calloc(pt->num_nodes, sizeof(*pt->physical_nodes[n].latencies));
    }    

    if ((rc = discover_mc_pci_topology(cpu_model, pt)) != E_SUCCESS) {
        goto err;
    }

    for (i=0; i<pt->num_nodes; i++) {
        for (j=0; j<pt->num_nodes; j++) {
            pt->physical_nodes[i].latencies[j] = measure_latency(cpu_model, i, j);
            if (i == j) {
                pt->physical_nodes[n].local_latency = pt->physical_nodes[i].latencies[j];
            }
        }
        discover_throttle_values(&pt->physical_nodes[i], &pt->physical_nodes[i].bw_throttle);
    }

    *physical_topology = pt;
    return E_SUCCESS;

err:
    return rc;
}

/** 
 * \brief Physical node topology to XML DOM tree
 */
int physical_topology_to_xml_doc(physical_topology_t* physical_topology, xmlDocPtr* doc)
{
    physical_node_t* physical_node;
    xmlTextWriterPtr writer;
    int rc;
    int n, k, j;

    writer = xmlNewTextWriterDoc(doc, 0);
    if (writer == NULL) {
        DBG_LOG(ERROR, "Error creating the xml writer\n");
        rc = E_ERROR;
        goto err;   
    }

    if ((rc = xmlTextWriterStartDocument(writer, NULL, MY_ENCODING, NULL)) < 0) {
        DBG_LOG(ERROR, "Error at xmlTextWriterStartDocument\n\n");
        rc = E_ERROR;
        goto err;   
    }

    if ((rc = xmlTextWriterStartElement(writer, BAD_CAST "physical_topology")) < 0) {
        DBG_LOG(ERROR, "Error at xmlTextWriterStartElement\n");
        rc = E_ERROR;
        goto err;   
    }

    for (n=0; n<physical_topology->num_nodes; n++) {
        physical_node = &physical_topology->physical_nodes[n];
        if ((rc = xmlTextWriterStartElement(writer, BAD_CAST "node")) < 0) {
            DBG_LOG(ERROR, "Error at xmlTextWriterStartElement\n");
            rc = E_ERROR;
            goto err;   
        }

        rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "node_id", "%d", n);
        if (rc < 0) {
            DBG_LOG(ERROR, "Error at xmlTextWriterWriteAttribute\n");
            rc = E_ERROR;
            goto err;   
        }

        /* dump memory latencies */
        if ((rc = xmlTextWriterStartElement(writer, BAD_CAST "mem_latencies")) < 0) {
            rc = E_ERROR;
            goto err;
        }
        for (k=0; k<physical_topology->num_nodes; k++) {
            if ((rc = xmlTextWriterStartElement(writer, BAD_CAST "latency")) < 0) {
                DBG_LOG(ERROR, "Error at xmlTextWriterStartElement\n");
                rc = E_ERROR;
                goto err;
            }
            if ((rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "node_id", "%d", k)) < 0) {
                DBG_LOG(ERROR, "Error at xmlTextWriterWriteAttribute\n");
                rc = E_ERROR;
                goto err;
            }
            if ((rc = xmlTextWriterWriteFormatString(writer, "%d", physical_topology->physical_nodes[n].latencies[k])) < 0) {
                DBG_LOG(ERROR, "Error at xmlTextWriterWriteFormatString\n");
                rc = E_ERROR;
                goto err;
            }
            if ((rc = xmlTextWriterEndElement(writer)) < 0) {
                DBG_LOG(ERROR, "Error at xmlTextWriterEndElement\n");
                rc = E_ERROR;
                goto err;
            }
        }

        if ((rc = xmlTextWriterEndElement(writer)) < 0) {
            DBG_LOG(ERROR, "Error at xmlTextWriterEndElement\n");
            rc = E_ERROR;
            goto err;
        }

        /* dump memory channels */ 
        if ((rc = xmlTextWriterStartElement(writer, BAD_CAST "mem_channels")) < 0) {
            rc = E_ERROR;
            goto err;
        }

        pci_regs_t *regs = physical_node->mc_pci_regs;
        for (j=0; regs != NULL && j < regs->channels; ++j) {
            if ((rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "channel", "%x:%x.%x",
                        regs->addr[j].bus_id, regs->addr[j].dev_id, regs->addr[j].funct)) < 0) 
            {
                DBG_LOG(ERROR, "Error at xmlTextWriterWriteFormatString\n");
                rc = E_ERROR;
                goto err;
            }
        }

        if ((rc = xmlTextWriterEndElement(writer)) < 0) {
            DBG_LOG(ERROR, "Error at xmlTextWriterEndElement\n");
            rc = E_ERROR;
            goto err;
        }

        /* dump throttle-register-value/bandwidth pairs */
        if ((rc = bw_throttle_to_xml(writer, physical_node->bw_throttle)) < 0) {
            rc = E_ERROR;
            goto err;
        }

        /* end node element */
        if ((rc = xmlTextWriterEndElement(writer)) < 0) {
            DBG_LOG(ERROR, "Error at xmlTextWriterEndElement\n");
            rc = E_ERROR;
            goto err;
        }
    }

    /* end topology element */
    if ((rc = xmlTextWriterEndElement(writer)) < 0) {
        DBG_LOG(ERROR, "Error at xmlTextWriterEndElement\n");
        rc = E_ERROR;
        goto err;
    }

    if ((rc = xmlTextWriterEndDocument(writer)) < 0) {
        DBG_LOG(ERROR, "testXmlwriterDoc: Error at xmlTextWriterEndDocument\n");
        rc = E_ERROR;
        goto err;
    }

    return E_SUCCESS;

err:
    return rc;
}

/** 
 * \brief Physical node topology to XML DOM tree file
 */
int physical_topology_to_xml(physical_topology_t* physical_topology, const char* xml_path)
{
    xmlDocPtr doc;
    physical_topology_to_xml_doc(physical_topology, &doc);
    xmlSaveFormatFileEnc(xml_path, doc, MY_ENCODING, 1);
    xmlFreeDoc(doc);
    return E_SUCCESS;
}


static void mem_channels_from_xml(xmlNode* root, physical_node_t* pn)
{
    int bus_id, dev_id, funct;
    xmlChar* tmp_xchar;
    xmlNode* cur_node = NULL;
    int channel = 0;
    pci_regs_t* regs = (pci_regs_t*) malloc(sizeof(pci_regs_t));
    pn->mc_pci_regs = regs;

    for (cur_node = root->children; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            tmp_xchar = xmlNodeGetContent(cur_node);
            sscanf((const char*) tmp_xchar, "%x:%x.%x", &bus_id, &dev_id, &funct);
            regs->addr[channel].bus_id = bus_id; 
            regs->addr[channel].dev_id = dev_id; 
            regs->addr[channel].funct = funct;
            ++channel;
            regs->channels = channel;
            xmlFree(tmp_xchar);
        }
    }
}

static void mem_latencies_from_xml(xmlNode* root, physical_topology_t* pt, physical_node_t* pn)
{
    xmlChar* tmp_xchar;
    xmlNode* cur_node = NULL;
    pn->latencies = calloc(pt->num_nodes, sizeof(*pn->latencies));
    for (cur_node = root->children; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            tmp_xchar = xmlGetProp(cur_node, BAD_CAST "node_id");
            int node_id = atoi((const char*) tmp_xchar);
            xmlFree(tmp_xchar);
            tmp_xchar = xmlNodeGetContent(cur_node);
            pn->latencies[node_id] = atoi((const char*) tmp_xchar);
            xmlFree(tmp_xchar);
        }
    }
    pn->local_latency = pn->latencies[pn->node_id];
}

static void physical_node_from_xml(xmlNode* root, cpu_model_t* cpu_model, physical_topology_t* pt)
{
    xmlNode *cur_node = NULL;
    xmlChar* tmp_xchar;

    tmp_xchar = xmlGetProp(root, BAD_CAST "node_id");
    int node_id = atoi((const char*) tmp_xchar);
    xmlFree(tmp_xchar);

    pt->physical_nodes[node_id].node_id = node_id;
    pt->physical_nodes[node_id].cpu_bitmask = numa_allocate_cpumask();
    pt->physical_nodes[node_id].cpu_model = cpu_model;
    numa_node_to_cpus(node_id, pt->physical_nodes[node_id].cpu_bitmask);
    pt->physical_nodes[node_id].num_cpus = num_cpus(pt->physical_nodes[node_id].cpu_bitmask);

    for (cur_node = root->children; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(BAD_CAST "mem_latencies", cur_node->name) == 0) {
                mem_latencies_from_xml(cur_node, pt, &pt->physical_nodes[node_id]);
            }
            if (xmlStrcmp(BAD_CAST "mem_channels", cur_node->name) == 0) {
                mem_channels_from_xml(cur_node, &pt->physical_nodes[node_id]);
            }
            if (xmlStrcmp(BAD_CAST "mem_bw", cur_node->name) == 0) {
                bw_throttle_from_xml(cur_node, &pt->physical_nodes[node_id].bw_throttle);
            }
        }
    }
}

/** 
 * \brief Loads the physical node topology from an XML file
 */
int physical_topology_from_xml(cpu_model_t* cpu_model, const char* xml_path, physical_topology_t** physical_topology)
{
    int rc;
    physical_topology_t* pt;

    xmlDocPtr doc;
    xmlNode *root_element = NULL;
    xmlNode *cur_node = NULL;
    xmlNode *topology_node = NULL;


    if (!(pt = malloc(sizeof(**physical_topology)))) {
        rc = E_NOMEM;
        goto err;
    }

    doc = xmlReadFile(xml_path, NULL, 0);
    root_element = xmlDocGetRootElement(doc);
    int num_nodes = 0;
    for (cur_node = root_element; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp((const unsigned char*) "physical_topology", cur_node->name) == 0) {
                topology_node = cur_node->children;
                /* find number of nodes */
                for (cur_node = topology_node; cur_node; cur_node = cur_node->next) {
                    if (cur_node->type == XML_ELEMENT_NODE) {
                        if (xmlStrcmp(BAD_CAST "node", cur_node->name) == 0) {
                            num_nodes++;
                        }
                    }
                }
                pt->num_nodes = num_nodes;
                if (!(pt->physical_nodes = calloc(pt->num_nodes, sizeof(*pt->physical_nodes)))) {
                    DBG_LOG(ERROR, "Failed physical nodes allocation\n");
                    abort();
                }

                /* process each node */
                for (cur_node = topology_node; cur_node; cur_node = cur_node->next) {
                    if (cur_node->type == XML_ELEMENT_NODE) {
                        if (xmlStrcmp((const unsigned char*) "node", cur_node->name) == 0) {
                            physical_node_from_xml(cur_node, cpu_model, pt);
                        }
                    }
                }

                break;
            }
        }
    }

    *physical_topology = pt;

    return E_SUCCESS;

err:
    return rc;
}

static const char* physical_topology_filename(config_t* cfg)
{
    const char* filename;

    if (config_lookup_string(cfg, "general.physical_topology", 
            &filename) == CONFIG_FALSE) 
    {
        return NULL;
    }

    return filename;
}

int load_physical_topology(config_t* cfg, physical_topology_t** physical_topology)
{
    const char* pt_filename;
    cpu_model_t* cpu;

    if (!(pt_filename = physical_topology_filename(cfg))) {
        return E_ERROR;
    }
    if ((cpu = cpu_model()) == NULL) {
        return E_ERROR;
    }

    physical_topology_from_xml(cpu, pt_filename, physical_topology);

    return E_SUCCESS;
}

int discover_and_save_physical_topology(const char* filename)
{
    physical_topology_t* pt;
    cpu_model_t* cpu;

    if ((cpu = cpu_model()) == NULL) {
        return E_ERROR;
    }
    discover_physical_topology(cpu, &pt);
    return physical_topology_to_xml(pt, filename);
}

void crawl_virtual_topology(
    virtual_topology_t* vt,
    void (*node_cb)(virtual_topology_element_t* vte),
    void (*nvm_cb)(virtual_topology_element_t* vte))
{
    virtual_topology_element_t* cur_vte;
    virtual_topology_element_t* tmp;

    graph_t* g = graph_create(vt->num_elements);

    HASH_ITER(hh, vt->elements_ht, cur_vte, tmp) {
        int i;
        for (i = 0; i < cur_vte->dep_count; i++) {
            graph_add_edge(g, cur_vte->dep[i]->id, cur_vte->id);
        }
    }

    int* topo_sort = graph_topo_sort(g);
    int i;
    for (i = 0; i < graph_vertex_count(g); i++) {
        int v = topo_sort[i];
        virtual_topology_element_t* vte = vt->elements_ar[v];
        if (node_cb && string_prefix("node", vte->name)) {
            node_cb(vte);
            continue;
        }
        if (nvm_cb && string_prefix("nvm", vte->name)) {
            nvm_cb(vte);
            continue;
        }
    }
}

static void create_virtual_node(virtual_topology_element_t* vte)
{
    int id;
    int cpunodebind;
    int membind;
    const char* nvmbind;
    if (!(config_setting_lookup_int(vte->cfg, "cpunodebind", &cpunodebind) &&
          config_setting_lookup_int(vte->cfg, "membind", &membind) &&
          config_setting_lookup_string(vte->cfg, "nvmbind", &nvmbind)))
    {
        return;
    }
    sscanf(vte->name, "node%d", &id);

    virtual_topology_element_t* vte_nvm;
    HASH_FIND_STR(vte->vt->elements_ht, nvmbind, vte_nvm);

    virtual_node_t* v_node = malloc(sizeof(*v_node));
    v_node->name = vte->name;
    v_node->node_id = id;
    v_node->cpunodebind = cpunodebind;
    v_node->membind = membind;
    v_node->nvm = (virtual_nvm_t*) vte_nvm->element;
    v_node->dram_node = &vte->vt->pt->physical_nodes[membind];

    vte->element = v_node;

    return;
}

static void create_virtual_nvm(virtual_topology_element_t* vte)
{
    int id;
    const char* mountpath;
    const char* sizestr;
    int membind;
    if (!(config_setting_lookup_string(vte->cfg, "mount", &mountpath) &&
          config_setting_lookup_string(vte->cfg, "size", &sizestr) &&
          config_setting_lookup_int(vte->cfg, "membind", &membind)))
    {
        return;
    }
    sscanf(vte->name, "nvm%d", &id);

    virtual_nvm_t* v_nvm = malloc(sizeof(*v_nvm));
    v_nvm->name = vte->name;
    v_nvm->nvm_id = id;
    v_nvm->size = string_to_size(sizestr);
    v_nvm->mountpath = mountpath;
    v_nvm->membind = membind;
    v_nvm->phys_node = &vte->vt->pt->physical_nodes[membind];
    vte->element = v_nvm;

    return;
}


int create_virtual_topology(config_t* cfg, physical_topology_t* pt, virtual_topology_t** vtp)
{
    config_setting_t* root;
    config_setting_t* setting;
    root = config_root_setting(cfg);

    setting = config_setting_get_member(root, "virtual_topology");

    virtual_topology_t* vt = malloc(sizeof(virtual_topology_t));

    vt->pt = pt;

    if (setting != NULL) {
        int i;

        /* create all virtual topology element place-holder objects */
        vt->num_elements = config_setting_length(setting);
        vt->elements_ht = NULL;
        vt->elements_ar = calloc(vt->num_elements, sizeof(*vt->elements_ar));
        for (i = 0; i < vt->num_elements; ++i) {
            config_setting_t *element = config_setting_get_elem(setting, i);
            virtual_topology_element_t* vte = malloc(sizeof(virtual_topology_element_t));
            vte->id = i;
            vte->name = element->name;
            vte->cfg = element;
            vte->dep_count = 0;
            vte->dep = NULL;
            vte->vt = vt;
            vt->elements_ar[i] = vte;
            HASH_ADD_STR(vt->elements_ht, name, vte);
        }
 
        /* add dependencies between virtual topology element objects */
        for (i = 0; i < vt->num_elements; ++i) {
            config_setting_t *element = config_setting_get_elem(setting, i);
            int key_count = config_setting_length(element);
            int j;
            virtual_topology_element_t* cur_vte;
            HASH_FIND_STR(vt->elements_ht, element->name, cur_vte);
            assert(cur_vte);
            for (j = 0; j < key_count; j++) {
                config_setting_t *key = config_setting_get_elem(element, j);
                if (key->type == CONFIG_TYPE_STRING) {
                    virtual_topology_element_t* vte;
                    const char* val = config_setting_get_string(key);
                    HASH_FIND_STR(vt->elements_ht, val, vte);
                    if (vte) {
                        cur_vte->dep = realloc(cur_vte->dep, sizeof(*cur_vte->dep) * (cur_vte->dep_count+1));
                        cur_vte->dep[cur_vte->dep_count++] = vte;
                    }
                }
            }
        }

        /* create all virtual topology element objects */
        crawl_virtual_topology(vt, create_virtual_node, create_virtual_nvm);

    }

    *vtp = vt;    

    return E_SUCCESS; 
}

int destroy_virtual_topology(virtual_topology_t* vt)
{
    // TODO: deallocate memory associated with all virtual topology objects
    return E_SUCCESS;
}

virtual_node_t* virtual_node(virtual_topology_t* vt, int node_id)
{
    int j;

    for (j = 0; j < vt->num_elements; j++) {
        virtual_topology_element_t* cur_vte = vt->elements_ar[j];
        if (string_prefix("node", cur_vte->name)) {
            virtual_node_t* v_node = cur_vte->element;
            if (v_node->node_id == node_id) {
                return v_node;
            }
        }
    }

    return NULL;
}

int num_virtual_nodes(virtual_topology_t* vt, int node_id)
{
    int j;
    int count = 0;

    for (j = 0; j < vt->num_elements; j++) {
        virtual_topology_element_t* cur_vte = vt->elements_ar[j];
        if (string_prefix("node", cur_vte->name)) {
            count++;
        }
    }

    return count;
}

virtual_node_t* virtual_node_iterator_first(virtual_node_iterator_t* it, virtual_topology_t* vt)
{
    it->vt = vt;
    it->i = -1;
    return virtual_node_iterator_next(it);
}

virtual_node_t* virtual_node_iterator_next(virtual_node_iterator_t* it)
{
    int i;

    for (i=it->i+1; i < it->vt->num_elements; i++)
    {
        virtual_topology_element_t* cur_vte = it->vt->elements_ar[i];
        if (string_prefix("node", cur_vte->name)) {
            it->i = i;
            return (virtual_node_t*) it->vt->elements_ar[it->i]->element;
        }
    }
    it->i = it->vt->num_elements;
    return NULL;
}

int virtual_node_iterator_done(virtual_node_iterator_t* it)
{
    if (it->i == it->vt->num_elements) return 1;

    return 0;
}

virtual_nvm_t* virtual_nvm_iterator_first(virtual_nvm_iterator_t* it, virtual_topology_t* vt)
{
    it->vt = vt;
    it->i = -1;
    return virtual_nvm_iterator_next(it);
}

virtual_nvm_t* virtual_nvm_iterator_next(virtual_nvm_iterator_t* it)
{
    int i;

    for (i=it->i+1; i < it->vt->num_elements; i++)
    {
        virtual_topology_element_t* cur_vte = it->vt->elements_ar[i];
        if (string_prefix("nvm", cur_vte->name)) {
            it->i = i;
            return (virtual_nvm_t*) it->vt->elements_ar[it->i]->element;
        }
    }
    it->i = it->vt->num_elements;
    return NULL;
}

int virtual_nvm_iterator_done(virtual_nvm_iterator_t* it)
{
    if (it->i == it->vt->num_elements) return 1;

    return 0;
}

int bind_process_on_virtual_node(virtual_topology_t* vt, int virtual_node_id)
{
    virtual_node_t* vnode = virtual_node(vt, virtual_node_id);
    int phys_node_id = vnode->dram_node->node_id;

    DBG_LOG(INFO, "Binding process on virtual node %d (physical node %d)\n", virtual_node_id, phys_node_id);

    struct bitmask* mask = numa_allocate_nodemask();
    numa_bitmask_setbit(mask, phys_node_id);
 
    numa_run_on_node_mask(mask);
    numa_set_membind(mask);
    numa_free_nodemask(mask);

    return E_SUCCESS;
}

int bind_thread_on_virtual_cpu(virtual_topology_t* vt, pid_t tid, int virtual_node_id, int virtual_cpu_id)
{
    virtual_node_t* vnode = virtual_node(vt, virtual_node_id);
    int phys_cpu_id = virtual_cpu_id_to_phys_cpu_id(vnode, virtual_cpu_id);
    DBG_LOG(INFO, "Binding thread tid %d on virtual processor %d (physical processor %d)\n", tid, virtual_cpu_id, phys_cpu_id);
    struct bitmask* cpubind = numa_allocate_cpumask();
    numa_bitmask_setbit(cpubind, virtual_cpu_id);
    if (numa_sched_setaffinity(tid, cpubind) != 0) {
        DBG_LOG(ERROR, "Cannot bind thread tid %d on virtual processor %d (physical processor %d)\n", tid, virtual_cpu_id, phys_cpu_id);
        numa_bitmask_free(cpubind);
        return E_ERROR;
    }
    numa_bitmask_free(cpubind);
    return E_SUCCESS;
}

#if 0
int bind_thread_on_mem(thread_manager_t* thread_manager, thread_t* thread, int virtual_node_id, int cpu_id)
{
    int physical_node_id;
    struct bitmask* membind = numa_allocate_nodemask();
    virtual_node_t* vnode = virtual_node(thread_manager->virtual_topology, virtual_node_id);
    physical_node_id = vnode->dram_node->node_id;
    numa_bitmask_setbit(membind, physical_node_id);
    numa_set_membind(membind);
    numa_free_nodemask(membind);

    return E_SUCCESS;
}

#endif
