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
#ifndef __MODEL_H
#define __MODEL_H

#include "config.h"
#include "cpu/cpu.h"
#include "thread.h"
#ifdef PAPI_SUPPORT
#include "cpu/pmc-papi.h"
#else
#include "cpu/pmc.h"
#endif

#define MAX_EPOCH_DURATION_US 1000000
#define MIN_EPOCH_DURATION_US 1

typedef struct {
	int enabled;
    int read_latency;
    int write_latency;
    int inject_delay;
#ifdef CALIBRATION_SUPPORT
    int calibration;
#endif
#ifdef PAPI_SUPPORT
    read_stalls_t pmc_stall_local;
    read_stalls_t pmc_stall_remote;
#else
    pmc_event_t* pmc_stall_cycles;
    pmc_event_t* pmc_remote_dram;
    int process_local_rank;
    int max_local_processe_ranks;
#endif

    double stalls_calibration_factor;
} latency_model_t;

extern latency_model_t latency_model;

typedef struct {
    unsigned int throttle_reg_val[MAX_THROTTLE_VALUE]; 
    double bandwidth[MAX_THROTTLE_VALUE];
    int npoints;
    int enabled;
} bw_model_t;

extern bw_model_t read_bw_model;
extern bw_model_t write_bw_model;

int init_bandwidth_model(config_t* cfg, struct virtual_topology_s* topology);
int init_latency_model(config_t* cfg, cpu_model_t* cpu, struct virtual_topology_s* virtual_topology);
void init_thread_latency_model(thread_t *thread);

void create_latency_epoch();

#endif /* __MODEL_H */
