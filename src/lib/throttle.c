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
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <libxml/xmlwriter.h>

#include "error.h"
#include "config.h"
#include "measure.h"
#include "monotonic_timer.h"
#include "model.h"
#include "stat.h"
#include "topology.h"
#include "throttle.h"
#include "cpu/cpu.h"

/**
 * \file
 * 
 * \page latency_emulation Memory bandwidth throttling
 * 
 * To emulate bandwidth, we rely on memory power throttling (supported by recent memory 
 * controllers) to limit the effective bandwidth to the DRAM attached to a socket.
 * Memory power throttling is configured through the PCI configuration space. 
 * We use a kernel-module to set the proper PCI registers. 
 * 
 * Initially, we perform a series of bandwidth measurements to find out the bandwidth 
 * that corresponds to each register value. We incrementally try out each register value 
 * starting from 0x800f until we saturate memory bandwidth.
 * 
 */ 


/**
 * \brief Derive the memory bandwidth throttle register values. 
 *
 * \details
 * Perform a series of bandwidth measurements to find out the bandwidth that 
 * corresponds to each throttle register value. The method incrementally tries
 * out each register value starting from 0x800f until it saturates memory 
 * bandwidth.
 *
 * The method creates a new bw_throttle. It is up to the caller to free the memory. 
 */
int discover_throttle_values(physical_node_t* phys_node, bw_throttle_t** bw_throttlep)
{
    double x[MAX_THROTTLE_VALUE];
    double best_rate;
    double m;
    int    i;
    bw_throttle_t* bw_throttle;
    uint16_t throttle_reg_val;
    int phys_node_id = phys_node->node_id;
    pci_regs_t *regs = phys_node->mc_pci_regs;
    int throttle_increment = 15;
    //int throttle_initial_value = 0x800f;
    int throttle_initial_value = 0x8feb;
    int min_number_throttle_pairs = 10;
    double stop_slope = 0.1;

    if (!(bw_throttle = malloc(sizeof(**bw_throttlep)))) {
        return E_NOMEM;
    }

    throttle_reg_val = throttle_initial_value;

    DBG_LOG(INFO, "throttle bus id %d, on physical node: %d\n", regs->addr[0].bus_id, phys_node_id);
    printf("throttle bus id %d, on physical node: %d\n", regs->addr[0].bus_id, phys_node_id);

    // run until bandwidth curve flattens out which we find out using 
    // gradient (slope) analysis 
    for (i=0; 
         i < MAX_THROTTLE_VALUE && throttle_reg_val < 0x8FFF; 
         i++, throttle_reg_val += throttle_increment) 
    {
        phys_node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, throttle_reg_val);
        best_rate = measure_read_bw(phys_node_id, phys_node_id);
        DBG_LOG(INFO, "throttle reg: 0x%x bandwidth: %f\n", throttle_reg_val, best_rate);
        printf("throttle reg: 0x%x bandwidth: %f\n", throttle_reg_val, best_rate);
        bw_throttle->throttle_reg_val[i] = throttle_reg_val;
        bw_throttle->bw[i] = best_rate;
        x[i] = (double) throttle_reg_val; // slope calculation requires values of type double
        if (i > min_number_throttle_pairs) {
            m = slope(&x[i-min_number_throttle_pairs], 
                      &bw_throttle->bw[i-min_number_throttle_pairs], 
                      min_number_throttle_pairs);
            if (abs(m) < stop_slope) {
                break;
            }
        }
    }

    // reset throttling
    phys_node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, 0x8FFF);

    bw_throttle->npairs = i;
    *bw_throttlep = bw_throttle;
    return E_SUCCESS;
}

int bw_throttle_to_xml(xmlTextWriterPtr writer, bw_throttle_t* bw_throttle) 
{
    int rc;
    int i;

    if ((rc = xmlTextWriterStartElement(writer, BAD_CAST "mem_bw")) < 0) {
        DBG_LOG(ERROR, "Error at xmlTextWriterStartElement\n");
        rc = E_ERROR;
        goto err;
    }

    for (i=0; i<bw_throttle->npairs; i++) 
    {
        if ((rc = xmlTextWriterStartElement(writer, BAD_CAST "throttle")) < 0) {
            DBG_LOG(ERROR, "Error at xmlTextWriterStartElement\n");
            rc = E_ERROR;
            goto err;
        }
        if ((rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "regval", "%x",
                    bw_throttle->throttle_reg_val[i])) < 0) 
        {
            DBG_LOG(ERROR, "Error at xmlTextWriterWriteFormatElement\n");
            rc = E_ERROR;
            goto err;
        }
        if ((rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "bw", "%f",
                    bw_throttle->bw[i])) < 0) 
        {
            DBG_LOG(ERROR, "Error at xmlTextWriterWriteFormatElement\n");
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

    return E_SUCCESS;

err:
    return rc;
}

int bw_throttle_pair_from_xml(xmlNode* root, unsigned int* throttle_reg_val, double* bw)
{
    xmlChar* tmp_xchar;
    xmlNode* cur_node = NULL;

    for (cur_node = root->children; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(BAD_CAST "regval", cur_node->name) == 0) {
                tmp_xchar = xmlNodeGetContent(cur_node);
                *throttle_reg_val = strtol((const char*) tmp_xchar, NULL, 16);
                xmlFree(tmp_xchar);
            }
            if (xmlStrcmp(BAD_CAST "bw", cur_node->name) == 0) {
                tmp_xchar = xmlNodeGetContent(cur_node);
                *bw = atof((const char*) tmp_xchar);
                xmlFree(tmp_xchar);
            }
        }
    }
    return E_SUCCESS;
}

int bw_throttle_from_xml(xmlNode* root, bw_throttle_t** bw_throttlep)
{
    bw_throttle_t* bw_throttle;
    xmlNode* cur_node = NULL;
    int n;

    if (!(bw_throttle = malloc(sizeof(**bw_throttlep)))) {
        return E_NOMEM;
    }

    for (n=0, cur_node = root->children; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(BAD_CAST "throttle", cur_node->name) == 0) {
                bw_throttle_pair_from_xml(cur_node, &bw_throttle->throttle_reg_val[n], &bw_throttle->bw[n]);
                n++;
            }
        }
    }
    bw_throttle->npairs = n;

    *bw_throttlep = bw_throttle;

    return E_SUCCESS;
}



#if 0
static int load_model(const char* path, const char* prefix, bw_model_t* bw_model)
{
    FILE *fp;
    char *line = NULL;
    char str[64];
    size_t len = 0;
    ssize_t read;
    int x;
    double y;
    int found_points;

    fp = fopen(path, "r");
    if (fp == NULL) {
        return E_ERROR;
    }

    DBG_LOG(INFO, "Loading %s bandwidth model from %s\n", prefix, path);
    for (found_points = 0; (read = getline(&line, &len, fp)) != -1; ) {
        if (strstr(line, prefix)) {
            sscanf(line, "%s\t%d\t%lf", str, &x, &y);
            DBG_LOG(INFO, "throttle reg: 0x%x, bandwidth: %f\n", x, y);
            bw_model->throttle_reg_val[found_points] = x;
            bw_model->bandwidth[found_points] = y;
            found_points++;
        }
    }
    free(line);
    if (found_points) {
        bw_model->npoints = found_points;
    } else {
        DBG_LOG(INFO, "No %s bandwidth model found in %s\n", prefix, path);
        return E_ERROR;
    }
    fclose(fp);
    return E_SUCCESS;
}

static int save_model(const char* path, const char* prefix, bw_model_t* bw_model)
{
    int i;
    FILE *fp;

    fp = fopen(path, "a");
    if (fp == NULL) {
        return E_ERROR;
    }

    DBG_LOG(INFO, "Saving %s bandwidth model into %s\n", prefix, path);
    for (i=0; i<bw_model->npoints; i++) {
        int x = bw_model->throttle_reg_val[i];
        double y = bw_model->bandwidth[i];
        //DBG_LOG(INFO, "throttle reg: 0x%x, bandwidth: %f\n", x, y);
        fprintf(fp, "%s\t%d\t%f\n", prefix, x, y);
    }
    fclose(fp);
    return E_SUCCESS;
}

static int find_data_point(bw_model_t* model, double target_bw, unsigned int* point)
{
    int i;
    double error;

    // go through all points as we are not sorted and pick the one closest
    *point = 0;
    error = target_bw;    
    for (i=1; i<model->npoints; i++) {
        if (fabs(model->bandwidth[i] - target_bw) < error) {
            *point = i;
            error = fabs(model->bandwidth[i] - target_bw);
        }
    }
    return E_SUCCESS;
}

int __set_write_bw(physical_node_t* node, uint64_t target_bw)
{
    pci_regs_t *regs = node->mc_pci_regs;
    int ret;
    unsigned int point;

    if (regs == NULL) {
        return E_SUCCESS;
    }

    if (target_bw == (uint64_t) (-1)) {
        node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, 0x8fff);
        return E_SUCCESS;
    }

    if ((ret = find_data_point(&write_bw_model, (double) target_bw, &point)) != E_SUCCESS) {
        return ret;
    }
    DBG_LOG(INFO, "Setting throttle reg: %d (0x%x), target write bandwidth: %" PRIu64 ", actual write bandwidth: %" PRIu64 "\n", write_bw_model.throttle_reg_val[point], write_bw_model.throttle_reg_val[point], target_bw, (uint64_t) write_bw_model.bandwidth[point]);
    node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, write_bw_model.throttle_reg_val[point]);
    
    return E_SUCCESS;
}

int set_write_bw(config_t* cfg, physical_node_t* node)
{
    int target_bw;
    __cconfig_lookup_int(cfg, "bandwidth.write", &target_bw);

    return __set_write_bw(node, target_bw);
}

int __set_read_bw(physical_node_t* node, uint64_t target_bw)
{
    pci_regs_t *regs = node->mc_pci_regs;
    int ret;
    unsigned int point;

    if (regs == NULL) {
        return E_SUCCESS;
    }

    if (target_bw == (uint64_t) (-1)) {
        node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, 0x8fff);
        return E_SUCCESS;
    }

    if ((ret = find_data_point(&read_bw_model, (double) target_bw, &point)) != E_SUCCESS) {
        return ret;
    }
    DBG_LOG(INFO, "Setting throttle reg: %d (0x%x), target read bandwidth: %" PRIu64 ", actual read bandwidth: %" PRIu64 "\n", read_bw_model.throttle_reg_val[point], read_bw_model.throttle_reg_val[point], target_bw, (uint64_t) read_bw_model.bandwidth[point]);
    node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, read_bw_model.throttle_reg_val[point]);

    return E_SUCCESS;
}

int set_read_bw(config_t* cfg, physical_node_t* node)
{
    int target_bw;
    __cconfig_lookup_int(cfg, "bandwidth.read", &target_bw);

    return __set_read_bw(node, target_bw);
}

int init_bandwidth_model(config_t* cfg, virtual_topology_t* topology)
{
    int i;
    char* model_file;

    srandom((int)monotonic_time());

    if (read_bw_model.enabled) {
        DBG_LOG(INFO, "Initializing bandwidth model\n");
        // initialize bandwidth model
        for (i=0; i<topology->num_virtual_nodes; i++) {
            // FIXME: currently we keep a single bandwidth model and not per-node bandwidth model
            physical_node_t* phys_node = topology->virtual_nodes[i].nvram_node;
            if (__cconfig_lookup_string(cfg, "bandwidth.model", &model_file) == CONFIG_TRUE) {
                if (load_model(model_file, "read", &read_bw_model) != E_SUCCESS) {
                    train_model(phys_node, 'r', &read_bw_model);
                    save_model(model_file, "read", &read_bw_model);
                }
                /*if (load_model(model_file, "write", &write_bw_model) != E_SUCCESS) {
                    train_model(phys_node, 'w', &write_bw_model);
                    save_model(model_file, "write", &write_bw_model);
                }*/
            }
        }

        // set read and write memory bandwidth 
        for (i=0; i<topology->num_virtual_nodes; i++) {
            physical_node_t* phys_node = topology->virtual_nodes[i].nvram_node;
            set_read_bw(cfg, phys_node);
            //set_write_bw(cfg, phys_node);
        }
    } else {
        // reset throttle registers
        for (i=0; i<topology->num_virtual_nodes; i++) {
            // FIXME: currently we keep a single bandwidth model and not per-node bandwidth model
            physical_node_t* phys_node = topology->virtual_nodes[i].dram_node;
            __set_read_bw(phys_node, (uint64_t) (-1));
            __set_write_bw(phys_node, (uint64_t) (-1));
        }
    }

    return E_SUCCESS;
}

#endif
