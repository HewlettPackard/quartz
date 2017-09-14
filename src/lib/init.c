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

#include <errno.h>
#include <unistd.h>
#include "cpu/cpu.h"
#include "config.h"
#include "error.h"
#include "model.h"
#include "measure.h"
#include "thread.h"
#include "topology.h"
#include "interpose.h"
#include "monotonic_timer.h"
#include "stat.h"

static void init() __attribute__((constructor));
static void finalize() __attribute__((destructor));

static physical_topology_t* physical_topology = NULL;
static virtual_topology_t* virtual_topology = NULL;

void finalize() {
    if (latency_model.enabled) {
        unregister_self();
    }

    uninit_bandwidth_model(virtual_topology);

#ifdef USE_STATISTICS
    stats_report();
#endif
    // finalize libraries and release resources
#ifdef PAPI_SUPPORT
    pmc_shutdown();
#endif

    //__cconfig_destroy(&cfg);
}

static int init_perf_model(config_t* cfg)
{
    cpu_model_t* cpu;

    if (init_interposition() != E_SUCCESS) {
        goto error;
    }

    if ((cpu = cpu_model()) == NULL) {
        DBG_LOG(ERROR, "No supported processor found\n");
        goto error;
    }

    CHECK_ERROR_CODE2(load_physical_topology(cfg, &physical_topology), goto error);
    CHECK_ERROR_CODE2(create_virtual_topology(cfg, physical_topology, &virtual_topology), goto error);

    int nodebind;
    if (__cconfig_lookup_int(cfg, "general.nodebind", &nodebind) == CONFIG_TRUE) {
        CHECK_ERROR_CODE2(bind_process_on_virtual_node(virtual_topology, nodebind), goto error);
    }

    CHECK_ERROR_CODE2(init_bandwidth_model(cfg, virtual_topology), goto error);
    CHECK_ERROR_CODE2(init_latency_model(cfg, cpu, virtual_topology), goto error);

    return E_SUCCESS;

error:
    return E_ERROR;
}

void init()
{
    config_t cfg;
    char* ld_preload_path;
    double start_time, end_time;

    start_time = monotonic_time_us();

    // we reset LD_PRELOAD to ensure we don't get into recursive preloads when 
    // calling popen during initialization. before exiting we reactivate LD_PRELOAD 
    // to allow LD_PRELOADs on children
    ld_preload_path = getenv("LD_PRELOAD");
    unsetenv("LD_PRELOAD");

    if (__cconfig_init(&cfg, "nvmemul.ini") == CONFIG_FALSE) {
        goto error;
    }

    if (dbg_init(&cfg, -1, NULL) != E_SUCCESS) {
        goto error;
    }

    if (init_perf_model(&cfg) != E_SUCCESS) {
        goto error; 
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
