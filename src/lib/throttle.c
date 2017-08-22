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
 * \page latency_emulation Memory power throttling
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
 * \brief Reset power throttling register
 */
void bw_throttle_reset(physical_node_t* phys_node)
{
    pci_regs_t *regs = phys_node->mc_pci_regs;
    phys_node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, 0x8FFF);
}

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
    int throttle_initial_value = 0x800f;
    int min_number_throttle_pairs = 10;
    double stop_slope = 0.1;

    if (!(bw_throttle = malloc(sizeof(**bw_throttlep)))) {
        return E_NOMEM;
    }

    throttle_reg_val = throttle_initial_value;

    DBG_LOG(INFO, "throttle bus id %d, on physical node: %d\n", regs->addr[0].bus_id, phys_node_id);

    // run until bandwidth curve flattens out which we find out using 
    // gradient (slope) analysis 
    for (i=0; 
         i < MAX_THROTTLE_VALUE && throttle_reg_val < 0x8FFF; 
         i++, throttle_reg_val += throttle_increment) 
    {
        phys_node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, throttle_reg_val);
        best_rate = measure_read_bw(phys_node_id, phys_node_id);
        DBG_LOG(INFO, "throttle reg: 0x%x bandwidth: %f\n", throttle_reg_val, best_rate);
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

    bw_throttle_reset(phys_node);

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


static int bw_throttle_find_pair(bw_throttle_t* bw_throttle, double target_bw, unsigned int* pair_index)
{
    int i;
    double error;

    // go through all pairs as we are not (necessarily) sorted and pick the one closest
    *pair_index = 0;
    error = target_bw;    
    for (i=1; i<bw_throttle->npairs; i++) {
        if (fabs(bw_throttle->bw[i] - target_bw) < error) {
            *pair_index = i;
            error = fabs(bw_throttle->bw[i] - target_bw);
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

    if ((ret = bw_throttle_find_pair(node->bw_throttle, (double) target_bw, &point)) != E_SUCCESS) {
        return ret;
    }

    DBG_LOG(INFO, "Setting throttle reg: %d (0x%x), target write bandwidth: %" PRIu64 ", actual write bandwidth: %" PRIu64 "\n", node->bw_throttle->throttle_reg_val[point], node->bw_throttle->throttle_reg_val[point], target_bw, (uint64_t) node->bw_throttle->bw[point]);
    node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, node->bw_throttle->throttle_reg_val[point]);
    
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

    if ((ret = bw_throttle_find_pair(node->bw_throttle, (double) target_bw, &point)) != E_SUCCESS) {
        return ret;
    }

    DBG_LOG(INFO, "Setting throttle reg: %d (0x%x), target read bandwidth: %" PRIu64 ", actual read bandwidth: %" PRIu64 "\n", node->bw_throttle->throttle_reg_val[point], node->bw_throttle->throttle_reg_val[point], target_bw, (uint64_t) node->bw_throttle->bw[point]);
    node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, node->bw_throttle->throttle_reg_val[point]);

    return E_SUCCESS;
}

int set_read_bw(config_t* cfg, physical_node_t* node)
{
    int target_bw;
    __cconfig_lookup_int(cfg, "bandwidth.read", &target_bw);

    return __set_read_bw(node, target_bw);
}
