/***************************************************************************
Copyright 2016 Hewlett Packard Enterprise Development LP.  
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version. This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details. You
should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
***************************************************************************/
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
                cpu_model->get_throttle_register(regs_addr[i], THROTTLE_DDR_ACT, &throttle_reg_val);
                if (throttle_reg_val < 0x8fff)
                    cpu_model->set_throttle_register(regs_addr[i], THROTTLE_DDR_ACT, 0x8fff);
            } else {
                cpu_model->set_throttle_register(regs_addr[i], THROTTLE_DDR_ACT, 0x800f);
            }
        }
        
        // measure local bandwidth of each node
        max_local_rbw = 0;
        for (i=0; i<physical_topology->num_nodes; i++) {
            physical_node_t* node_i = &physical_topology->physical_nodes[i];
            rbw = measure_read_bw(node_i->node_id, node_i->node_id);
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


/**
 * \brief Construct a virtual topology
 *
 * Constructs a NUMA virtual topology where two physical sockets are fused into a 
 * single virtual node
 */
int init_numa_topology(char* physical_nodes_str, int hyperthreading, char* mc_pci_file, cpu_model_t* cpu_model, virtual_topology_t** virtual_topologyp)
{
#if 0
    char* saveptr = NULL;
    char* token = "NULL";
    int* physical_node_ids;
    physical_node_t** physical_nodes = NULL;
    int num_physical_nodes;
    int n, v, i, j, sibling_idx;
    int node_id;
    physical_node_t* node_i, *node_j, *sibling_node;
    int ret;
    int min_distance;
    struct bitmask* mem_nodes;
    virtual_topology_t* virtual_topology;

    DBG_LOG(DEBUG, "Possible NUMA nodes are %d\n", numa_num_possible_nodes());
    DBG_LOG(DEBUG, "NUMA nodes allowed are %lu\n", numa_get_mems_allowed()->size);
    DBG_LOG(DEBUG, "NUMA configured CPUs are %d\n", numa_num_configured_cpus());

    // parse the physical nodes string
    physical_node_ids = calloc(numa_num_possible_nodes(), sizeof(*physical_node_ids));
    num_physical_nodes = 0;

    while ((token = strtok_r(physical_nodes_str, ",", &saveptr))) {
        physical_node_ids[num_physical_nodes] = atoi(token);
        physical_nodes_str = NULL;
        if (++num_physical_nodes > numa_num_possible_nodes()) {
            // we re being asked to run on more nodes than available
            free(physical_node_ids);
            ret = E_ERROR;
            goto done;
        }
    }
    if (!(physical_nodes = calloc(num_physical_nodes, sizeof(*physical_nodes)))) {
        DBG_LOG(ERROR, "Failed physical nodes allocation\n");
        abort();
    }

    // select those nodes we can run on (e.g. not constrained by any numactl)
    mem_nodes = numa_get_mems_allowed();
    for (i=0, n=0; i<num_physical_nodes; i++) {
        node_id = physical_node_ids[i];
        if (numa_bitmask_isbitset(mem_nodes, node_id)) {
            physical_nodes[n] = malloc(sizeof(**physical_nodes));
            memset(physical_nodes[n], 0, sizeof(**physical_nodes));
            physical_nodes[n]->node_id = node_id;
            physical_nodes[n]->cpu_bitmask = numa_allocate_cpumask();
            physical_nodes[n]->cpu_model = cpu_model;
            numa_node_to_cpus(node_id, physical_nodes[n]->cpu_bitmask);
            if (hyperthreading) {
                physical_nodes[n]->num_cpus = num_cpus(physical_nodes[n]->cpu_bitmask);
            } else {
                DBG_LOG(INFO, "Not using hyperthreading.\n");
                // disable the upper half of the processors in the bitmask
                physical_nodes[n]->num_cpus = num_cpus(physical_nodes[n]->cpu_bitmask) / 2;
                int fc = first_cpu(physical_nodes[n]->cpu_bitmask);
                for (j=fc+system_num_cpus()/2; j<fc+system_num_cpus()/2+physical_nodes[n]->num_cpus; j++) {
                    if (numa_bitmask_isbitset(physical_nodes[n]->cpu_bitmask, j)) {
                        numa_bitmask_clearbit(physical_nodes[n]->cpu_bitmask, j);
                    }
                }
            }
            DBG_LOG(INFO, "%d CPUs on physical node %d\n", physical_nodes[n]->num_cpus, n);
            n++;
        }
    }
    free(physical_node_ids);
    num_physical_nodes = n;

    // If pci bus topology of each physical node is not provided then discover it.
    // The bus topology must be always known even if BW model is disabled.
    if (mc_pci_file == NULL ||
          (mc_pci_file &&
          load_mc_pci_topology(mc_pci_file, physical_nodes, num_physical_nodes) != E_SUCCESS))
    {
        //discover_mc_pci_topology(cpu_model, physical_nodes, num_physical_nodes);
        save_mc_pci_topology(mc_pci_file, physical_nodes, num_physical_nodes);
        DBG_LOG(INFO, "Topology MC PCI file saved, restart the process\n");
        exit(0);
    }

    // form virtual nodes by grouping physical nodes that are close to each other
    virtual_topology = malloc(sizeof(*virtual_topology));
    virtual_topology->num_virtual_nodes = num_physical_nodes / 2 + num_physical_nodes % 2;
    virtual_topology->virtual_nodes = calloc(virtual_topology->num_virtual_nodes, 
                                             sizeof(*(virtual_topology->virtual_nodes)));

    DBG_LOG(INFO, "Number of physical nodes %d\n", num_physical_nodes);
    DBG_LOG(INFO, "Number of virtual nodes %d\n", virtual_topology->num_virtual_nodes);

    for (i=0, v=0; i<num_physical_nodes; i++) {
        min_distance = INT_MAX;
        sibling_node = NULL;
        sibling_idx = -1;
        if ((node_i = physical_nodes[i]) == NULL) {
            continue;
        }

        for (j=i+1; j<num_physical_nodes; j++) {
            if ((node_j = physical_nodes[j]) == NULL) {
                continue;
            }
            // TODO: numa_distance() returns '0' on error
            if (numa_distance(node_i->node_id,node_j->node_id) < min_distance) {
                sibling_node = node_j;
                sibling_idx = j;
            }
        }

        if (sibling_node) {
            physical_nodes[i] = physical_nodes[sibling_idx] = NULL;
            virtual_node_t* virtual_node = &virtual_topology->virtual_nodes[v];
            virtual_node->dram_node = node_i;
            virtual_node->nvram_node = sibling_node;
            virtual_node->dram_node->latency = measure_latency(cpu_model,
                                                               virtual_node->dram_node->node_id,
                                                               virtual_node->dram_node->node_id);
            virtual_node->nvram_node->latency = measure_latency(cpu_model,
                                                                virtual_node->dram_node->node_id,
                                                                virtual_node->nvram_node->node_id);
            virtual_node->node_id = v;
            DBG_LOG(INFO, "Fusing physical nodes %d %d into virtual node %d\n", 
                    node_i->node_id, sibling_node->node_id, virtual_node->node_id);
            v++;
        }
    }

    // any physical node that is not paired with another physical node is 
    // formed into a virtual node on its own
    if (2*v < num_physical_nodes) {
        for (i=0; i<num_physical_nodes; i++) {
            node_i = physical_nodes[i];
            virtual_node_t* virtual_node = &virtual_topology->virtual_nodes[v];
            virtual_node->dram_node = virtual_node->nvram_node = node_i;
            virtual_node->node_id = v;
            virtual_node->dram_node->latency = measure_latency(cpu_model,
                                                               virtual_node->dram_node->node_id,
                                                               virtual_node->dram_node->node_id);
            DBG_LOG(WARNING, "Forming physical node %d into virtual node %d without a sibling node.\n",
                    node_i->node_id, virtual_node->node_id);
        }
    }

    *virtual_topologyp = virtual_topology;
    ret = E_SUCCESS;
done:
    free(physical_nodes);
    return ret;
#endif
    return 0;
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
    v_nvm->nvm_node = &vte->vt->pt->physical_nodes[membind];
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

/**
 * \brief Initialize virtual topology
 *
 */
int init_virtual_topology(config_t* cfg, cpu_model_t* cpu_model, virtual_topology_t** vtp)
{
    int rc;
    physical_topology_t* pt;
    char* physical_topology_filename;

    if (__cconfig_lookup_string(cfg, "general.physical_topology", 
            &physical_topology_filename) == CONFIG_FALSE) 
    {
        return E_ERROR;
    }

    CHECK_ERROR_CODE(physical_topology_from_xml(cpu_model, physical_topology_filename, &pt));


    
/*
    char* physical_nodes;
    int   hyperthreading;
    char* mc_pci_file;
    
    if (__cconfig_lookup_string(cfg, "topology.physical_nodes", &physical_nodes) == CONFIG_FALSE) {
        return E_ERROR;
    }


    __cconfig_lookup_bool(cfg, "topology.hyperthreading", &hyperthreading);
    if (__cconfig_lookup_string(cfg, "topology.mc_pci", &mc_pci_file) == CONFIG_FALSE) {
        mc_pci_file = NULL;
    }

    return init_numa_topology(physical_nodes, hyperthreading, mc_pci_file, cpu_model, virtual_topologyp);
*/
}
