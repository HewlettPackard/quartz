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
#include "cpu/cpu.h"
#include "config.h"
#include "error.h"
#include "measure.h"
#include "stat.h"
#include "topology.h"
#include "monotonic_timer.h"
#include "model.h"

/**
 * \file
 * 
 * \page latency_emulation Memory bandwidth emulation
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


bw_model_t read_bw_model;
bw_model_t write_bw_model;


#define THROTTLE_INCREMENT 15
#define THROTTLE_INITIAL_VALUE 0x800f

static int train_model(physical_node_t* phys_node, char model_type, bw_model_t* bw_model)
{
    double x[MAX_THROTTLE_VALUE];
    double best_rate;
    double m;
    int    i;
    uint16_t    throttle_reg_val;

    int min_number_throttle_points = 10;
    double stop_slope = 0.1;
    int phys_node_id = phys_node->node_id;
    pci_regs_t *regs = phys_node->mc_pci_regs;

    // reset throttling
    phys_node->cpu_model->get_throttle_register(regs, THROTTLE_DDR_ACT, &throttle_reg_val);
    if (throttle_reg_val < 0x8fff)
        phys_node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, 0x8FFF);

    DBG_LOG(INFO, "throttle bus id %d, on physical node: %d\n", regs->addr[0].bus_id, phys_node_id);

    // we run until our bandwidth curve flattens out which we find out using 
    // gradient (slope) analysis 
    for (i=0; i < MAX_THROTTLE_VALUE; i++) {
        phys_node->cpu_model->get_throttle_register(regs, THROTTLE_DDR_ACT, &throttle_reg_val);
        if (throttle_reg_val >= 0x8fff) throttle_reg_val = THROTTLE_INITIAL_VALUE;
        else throttle_reg_val += THROTTLE_INCREMENT;
        if (model_type == 'r') {
            phys_node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, throttle_reg_val);
            best_rate = measure_read_bw(phys_node_id, phys_node_id);
            // restore throttling register
            //phys_node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, 0x8fff);
        } /*else if (model_type == 'w') {
            phys_node->cpu_model->set_throttle_register(bus_id, THROTTLE_DDR_ACT, throttle_reg_val);
            best_rate = measure_write_bw(phys_node_id, phys_node_id);
            // restore throttling register
            phys_node->cpu_model->set_throttle_register(bus_id, THROTTLE_DDR_ACT, 0x8fff);
        }*/
        DBG_LOG(INFO, "throttle reg: 0x%x, %c bandwidth: %f\n", throttle_reg_val, model_type, best_rate);
        bw_model->throttle_reg_val[i] = throttle_reg_val;
        bw_model->bandwidth[i] = best_rate;
        x[i] = (double) throttle_reg_val; // slope calculation requires values of type double
        if (i > min_number_throttle_points) {
            m = slope(&x[i-min_number_throttle_points], 
                      &bw_model->bandwidth[i-min_number_throttle_points], 
                      min_number_throttle_points);
            if (abs(m) < stop_slope) {
                break;
            }
        }
    }
    bw_model->npoints = i;
    return E_SUCCESS;
}

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
