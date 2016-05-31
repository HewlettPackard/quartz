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
#include <string.h>
#include "cpu/cpu.h"
#include "config.h"
#include "error.h"
#include "thread.h"
#include "topology.h"
#include "model.h"
#include "monotonic_timer.h"

/**
 * \file
 * 
 * \page latency_emulation Memory latency emulation
 * 
 * To emulate latency, we construct epochs and inject software created delays 
 * at the end of each epoch.
 * Epochs are created either at fixed intervals by periodically interrupting 
 * threads or on demand when a synchronization method (lock, unlock) is called.
 *
 * Delays are calculated using a simple analytic model that takes input from 
 * performance counters.
 */ 



latency_model_t latency_model;

#pragma GCC push_options
#pragma GCC optimize ("O0")
inline hrtime_t hrtime_cycles(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtscp" : "=a"(lo), "=d"(hi));
    return ( (hrtime_t)lo)|( ((hrtime_t)hi)<<32 );
}
#pragma GCC pop_options

/*
static inline hrtime_t ns_to_cycles(int cpu_speed_mhz, int ns)
{
    return (cpu_speed_mhz * ns) / 1000;
}
*/

inline hrtime_t cycles_to_us(int cpu_speed_mhz, hrtime_t cycles)
{
    return (cycles/cpu_speed_mhz);
}

#pragma GCC push_options
#pragma GCC optimize ("O0")
static inline void create_delay_cycles(hrtime_t cycles)
{
    hrtime_t start, stop;

    start = hrtime_cycles();
    do {
        stop = hrtime_cycles();
    } while (stop - start < cycles);
}
#pragma GCC pop_options

/*
static inline void create_delay_ns(cpu_model_t* cpu, int ns)
{
    hrtime_t cycles;
    cycles = ns_to_cycles(cpu, ns);
    create_delay_cycles(cycles);
}
*/

static int check_target_latency_against_hw_latency(virtual_topology_t* virtual_topology) {
    int status = 0;
    int i;
    int hw_latency_dram;
    int hw_latency_nvram;

    for (i = 0; i < virtual_topology->num_virtual_nodes; ++i) {
        hw_latency_dram = virtual_topology->virtual_nodes[i].dram_node->latency;
        hw_latency_nvram = virtual_topology->virtual_nodes[i].nvram_node->latency;
        if (hw_latency_dram >= latency_model.read_latency ||
            hw_latency_dram >= latency_model.write_latency ||
            hw_latency_nvram >= latency_model.read_latency ||
            hw_latency_nvram >= latency_model.write_latency) {
            DBG_LOG(ERROR, "Target read (%d) and write (%d) latency to be emulated must be greater than the "
            		"hardware latency dram (%d) and virtual nvram (%d) (virtual node %d)\n",
            		latency_model.read_latency, latency_model.write_latency, hw_latency_dram, hw_latency_nvram, i);
            status = -1;
            break;
        }
    }

    return status;
}

int init_latency_model(config_t* cfg, cpu_model_t* cpu, virtual_topology_t* virtual_topology)
{
	int i;

    DBG_LOG(INFO, "Initializing latency model\n");

    memset(&latency_model, 0, sizeof(latency_model_t));
    latency_model.enabled = 1;

    __cconfig_lookup_int(cfg, "latency.read", &latency_model.read_latency);
    __cconfig_lookup_int(cfg, "latency.write", &latency_model.write_latency);

    if (check_target_latency_against_hw_latency(virtual_topology) < 0) {
        return E_INVAL;
    }

    __cconfig_lookup_bool(cfg, "latency.inject_delay", &latency_model.inject_delay);
    if (!latency_model.inject_delay) {
        DBG_LOG(WARNING, "Latency model is enabled, but delay injection is disabled\n");
    }

#ifdef PAPI_SUPPORT
    if (pmc_init() != 0) {
        return E_ERROR;
    }

    latency_model.pmc_stall_local = cpu->pmc_events.read_stalls_events_local;
    latency_model.pmc_stall_remote = cpu->pmc_events.read_stalls_events_remote;
#else
    for (i=0; cpu->pmc_events->known_events[i].name; ++i) {
        // LDM_STALL_CYCLES implementation for each processor is mandatory
        if (strcasecmp(cpu->pmc_events->known_events[i].name, "LDM_STALL_CYCLES") == 0) {
            if (!(latency_model.pmc_stall_cycles = enable_pmc_event(cpu, "LDM_STALL_CYCLES"))) {
                return E_NOENT;
            }
        }
        if (strcasecmp(cpu->pmc_events->known_events[i].name, "REMOTE_DRAM") == 0) {
            if (!(latency_model.pmc_remote_dram = enable_pmc_event(cpu, "REMOTE_DRAM"))) {
                return E_NOENT;
            }
        }
    }

    assert(latency_model.pmc_stall_cycles);
#endif

#ifdef CALIBRATION_SUPPORT
    __cconfig_lookup_bool(cfg, "latency.calibration", &latency_model.calibration);
    if (latency_model.calibration) {
        latency_model.stalls_calibration_factor = 1.0;
    }
#endif

    return E_SUCCESS;
}

__thread uint64_t tls_overhead = 0;
__thread int tls_hw_local_latency = 0;
__thread int tls_hw_remote_latency = 0;
#ifdef MEMLAT_SUPPORT
__thread uint64_t tls_global_remote_dram = 0;
__thread uint64_t tls_global_local_dram = 0;
#endif

void init_thread_latency_model(thread_t *thread)
{
    tls_hw_local_latency = thread->virtual_node->dram_node->latency;
    tls_hw_remote_latency = thread->virtual_node->nvram_node->latency;
}

void create_latency_epoch()
{
    uint64_t stall_cycles = 0;
    uint64_t delay_cycles = 0;
    int hw_latency;
    int target_latency;
    hrtime_t start, stop;
    double epoch_end;

    start = hrtime_cycles();

    // An epoch may be created by a critical section and the static epoch
    // may interfere with the current epoch creation. Block the signal here
    // and unblock it at the end of this function.
    block_new_epoch();

    // must always be thread_self since we call core specific data through hrtime_cycles
    thread_t* thread = thread_self();

    if (!reached_min_epoch_duration(thread)) {
    	if (!thread) thread = thread_self();
    	if (thread) thread->signaled = 0;
    	unblock_new_epoch();
        return;
    }

    //DBG_LOG(INFO, "new epoch for thread id [%i]\n", thread->tid);

#ifdef USE_STATISTICS
    if (thread->thread_manager->stats.enabled) {
        thread->stats.epochs++;
    }
#endif

    // this is the generic hardware latency for this thread (it takes into account the current virtual node latencies)
    hw_latency = thread->virtual_node->nvram_node->latency;
    target_latency = latency_model.read_latency;

    // check if the thread_self is remote (virtual topology where dram != nvram) or local (dram == nvram)
    // on this case, stall cycles will be a proportion of remote memory accesses
    // TODO: the read pmc method used below must be changed to support PAPI
    if (thread->virtual_node->dram_node != thread->virtual_node->nvram_node &&
            latency_model.pmc_remote_dram) {
        stall_cycles = read_pmc_event(latency_model.pmc_remote_dram);
	} else {
		stall_cycles = read_pmc_event(latency_model.pmc_stall_cycles);
	}

#ifdef CALIBRATION_SUPPORT
    if (latency_model.calibration) {
        stall_cycles = (uint64_t)((double)stall_cycles * latency_model.stalls_calibration_factor);
    }
#endif

    delay_cycles = stall_cycles * ((double)(target_latency - hw_latency) / ((double) hw_latency));

    stop = hrtime_cycles();
    tls_overhead += stop - start;

    DBG_LOG(DEBUG, "overhead cycles: %lu; immediate overhead %lu; stall cycles: %lu; delay cycles: %lu\n", tls_overhead, stop - start, stall_cycles, delay_cycles);

    if (delay_cycles > tls_overhead) {
    	delay_cycles -= tls_overhead;
        tls_overhead = 0;
    }
    else {
    	tls_overhead -= delay_cycles;
    	delay_cycles = 0;
    }

#ifdef MEMLAT_SUPPORT
    thread->stall_cycles += stall_cycles;
#endif

#ifdef USE_STATISTICS
    if (thread->thread_manager->stats.enabled) {
        thread->stats.stall_cycles += stall_cycles;
        thread->stats.delay_cycles += delay_cycles;
        thread->stats.overhead_cycles = tls_overhead;
    }
#endif

    epoch_end = monotonic_time_us();

    DBG_LOG(DEBUG, "injecting delay of %lu cycles (%lu usec) - discounted overhead\n", delay_cycles,
                    cycles_to_us(thread->cpu_speed_mhz, delay_cycles));
    if (delay_cycles && latency_model.inject_delay) {
        create_delay_cycles(delay_cycles);
    }

#ifdef USE_STATISTICS
    if (thread->thread_manager->stats.enabled) {
    	uint64_t older_epoch_timestamp = thread->stats.last_epoch_timestamp;
    	uint64_t diff_epoch_timestamp = epoch_end - older_epoch_timestamp;

    	if (diff_epoch_timestamp < thread->stats.shortest_epoch_duration_us) {
    	    thread->stats.shortest_epoch_duration_us = diff_epoch_timestamp;
    	}

    	if (diff_epoch_timestamp > thread->stats.longest_epoch_duration_us) {
		    thread->stats.longest_epoch_duration_us = diff_epoch_timestamp;
    	}

    	thread->stats.overall_epoch_duration_us += diff_epoch_timestamp;
    	thread->stats.last_epoch_timestamp = monotonic_time_us();
    } else {
    	// last epoch timestamp must always be updated
        thread->stats.last_epoch_timestamp = monotonic_time_us();
    }
#else
    thread->last_epoch_timestamp = monotonic_time_us();
#endif
    // this must be the last step, since this function is called also from the signal handler
    // and the monitor thread sets this flag, we must make sure race conditions are prevented
    thread->signaled = 0;

    unblock_new_epoch();
}
