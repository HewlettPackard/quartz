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
#include <errno.h>
#include "cpu/cpu.h"
#include "config.h"
#include "error.h"
#include "model.h"
#include "measure.h"
#include "thread.h"
#include "topology.h"
#include "interpose.h"
#include "monotonic_timer.h"
#include "pflush.h"
#include "stat.h"

static void init() __attribute__((constructor));
static void finalize() __attribute__((destructor));

int set_process_local_rank();
int unset_process_local_rank();
int partition_cpus(virtual_topology_t* virtual_topology);

static virtual_topology_t* virtual_topology = NULL;

void finalize() {
    int i;
    if (latency_model.enabled) {
        unregister_self();
    }

    if (read_bw_model.enabled) {
        for (i=0; i < virtual_topology->num_virtual_nodes; i++) {
            // FIXME: currently we keep a single bandwidth model and not per-node BW model
            physical_node_t* phys_node = virtual_topology->virtual_nodes[i].nvram_node;
            pci_regs_t *regs = phys_node->mc_pci_regs;

            // reset throttling
            phys_node->cpu_model->set_throttle_register(regs, THROTTLE_DDR_ACT, 0x8FFF);
        }
    }
#ifdef USE_STATISTICS
    stats_report();
#endif
    // finalize libraries and release resources
#ifdef PAPI_SUPPORT
    pmc_shutdown();
#endif

    unset_process_local_rank();

    //__cconfig_destroy(&cfg);
}

void init()
{
    config_t cfg;
    cpu_model_t* cpu;
    char* ld_preload_path;
    double start_time, end_time;
#ifdef CALIBRATION_SUPPORT
    int i;
#endif

    // FIXME: do we need to register the main thread with our system?
    // YES: for sure for single-threaded apps

    start_time = monotonic_time_us();

    // we reset LD_PRELOAD to ensure we don't get into recursive preloads when 
    // calling popen during initialization. before exiting we reactivate LD_PRELOAD 
    // to allow LD_PRELOADS on children
    ld_preload_path = getenv("LD_PRELOAD");
    unsetenv("LD_PRELOAD");

    if (__cconfig_init(&cfg, "nvmemul.ini") == CONFIG_FALSE) {
        goto error;
    }

    __cconfig_lookup_bool(&cfg, "latency.enable", &latency_model.enabled);
    __cconfig_lookup_bool(&cfg, "bandwidth.enable", &read_bw_model.enabled);

    if (dbg_init(&cfg, -1, NULL) != E_SUCCESS) {
        goto error;
    }

    if (init_interposition() != E_SUCCESS) {
        goto error;
    }

    if ((cpu = cpu_model()) == NULL) {
        DBG_LOG(ERROR, "No supported processor found\n");
        goto error;
    }

    init_virtual_topology(&cfg, cpu, &virtual_topology);

    if (init_bandwidth_model(&cfg, virtual_topology) != E_SUCCESS) {
        goto error;
    }

    if (latency_model.enabled) {
        if (init_latency_model(&cfg, cpu, virtual_topology) != E_SUCCESS) {
   	        goto error;
        }

        init_thread_manager(&cfg, virtual_topology);

#ifdef USE_STATISTICS
        // statistics makes use of the thread manager and is used by the register_self()
        stats_enable(&cfg);
#endif

        set_process_local_rank();

        // thread manager must be initialized and local rank set
        // CPU partitioning must be made before the first thread is registered
        if (partition_cpus(virtual_topology) != E_SUCCESS) {
            goto error;
        }

        if (register_self() != E_SUCCESS) {
            goto error;
        }

#ifdef CALIBRATION_SUPPORT
        // main thread is now tracked by the latency emulator
        // first, calibrate the latency emulation
        if (latency_model.calibration) {
            for (i = 0; i < virtual_topology->num_virtual_nodes; ++i) {
                latency_calibration(&virtual_topology->virtual_nodes[i]);
            }
        }
#endif
        int write_latency;
        __cconfig_lookup_int(&cfg, "latency.write", &write_latency);
        init_pflush(cpu_speed_mhz(), write_latency);
    }

    end_time = monotonic_time_us();

#ifdef USE_STATISTICS
    if (latency_model.enabled) {
        stats_set_init_time(end_time - start_time);
    }
#endif

    if (ld_preload_path)
        setenv("LD_PRELOAD", ld_preload_path, 1);

    return;

error:
    /* Cannot initialize library -- catastrophic error */
    if (ld_preload_path)
        setenv("LD_PRELOAD", ld_preload_path, 1);

    fprintf(stderr, "ERROR: nvmemul: Initialization failed. Running without non-volatile memory emulation.\n");
}
