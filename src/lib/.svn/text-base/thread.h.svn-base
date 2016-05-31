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
#ifndef __THREAD_H
#define __THREAD_H

#include <sys/types.h>
#include <stdint.h>
#include <numa.h>
#include <pthread.h>
#include <libconfig.h>
#include "topology.h"
#include "cpu/cpu.h"
#include "stat.h"


struct thread_manager_s; // opaque

typedef uint64_t hrtime_t;

// TODO: Used by memlat benchmark, should be disabled on a release version
#define MEMLAT_SUPPORT

typedef struct thread_s {
    struct virtual_node_s* virtual_node;
    pthread_t pthread;
    pid_t tid;
    int cpu_id; // the processor the thread is bound on
    int cpu_speed_mhz;
    struct thread_manager_s* thread_manager;
    struct thread_s* next;
    int signaled;
#ifdef MEMLAT_SUPPORT
	uint64_t stall_cycles;
#endif
#ifdef USE_STATISTICS
    thread_stats_t stats;
#else
    double last_epoch_timestamp;
#endif
} thread_t;

typedef struct thread_manager_s {
    pthread_mutex_t mutex;
    thread_t* thread_list;
    int max_epoch_duration_us; // maximum epoch duration in microseconds
    int min_epoch_duration_us; // minimum epoch duration in microseconds
    int next_virtual_node_id; // used by the round-robin policy -- next virtual node to run on 
    int next_cpu_id; // used by the round-robin policy -- next cpu to run on
    struct virtual_topology_s* virtual_topology;   
#ifdef USE_STATISTICS
    stats_t stats;
#endif
} thread_manager_t; 

int init_thread_manager(config_t* cfg, struct virtual_topology_s* virtual_topology);
int register_self();
int unregister_self();
thread_t* thread_self();
int reached_min_epoch_duration(thread_t* thread);
void block_new_epoch();
void unblock_new_epoch();

#endif /* __THREAD_H */
