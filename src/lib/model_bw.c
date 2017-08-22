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
#include "monotonic_timer.h"
#include "model.h"
#include "stat.h"
#include "topology.h"

/**
 * \file
 * 
 * \page latency_emulation Memory bandwidth emulation
 * 
 * To emulate bandwidth, we rely on memory power throttling (supported by recent memory 
 * controllers) to limit the effective bandwidth to the DRAM attached to a socket.
 */ 

static int bw_model_enabled;

int init_bandwidth_model(config_t* cfg, virtual_topology_t* vt)
{
    int i;
    char* model_file;

    srandom((int)monotonic_time());

    // reset power throttling 
    for (i=0; i<vt->pt->num_nodes; i++) {
        physical_node_t* phys_node = &vt->pt->physical_nodes[i];
        bw_throttle_reset(phys_node);
    }

    __cconfig_lookup_bool(cfg, "bandwidth.enable", &bw_model_enabled);

    if (!bw_model_enabled) return E_SUCCESS;

    // set read and write memory bandwidth for nvm
    virtual_nvm_iterator_t itnvm;
    virtual_nvm_t* vnvm;
    for (vnvm = virtual_nvm_iterator_first(&itnvm, vt);
         !virtual_nvm_iterator_done(&itnvm);
         vnvm = virtual_nvm_iterator_next(&itnvm))
    {
        set_read_bw(cfg, vnvm->nvm_node);
        // write bandwidth throttling not supported
    }

    return E_SUCCESS;
}


int uninit_bandwidth_model(virtual_topology_t* vt)
{
    int i;

    if (!bw_model_enabled) return E_SUCCESS;

    // reset power throttling 
    for (i=0; i<vt->pt->num_nodes; i++) {
        physical_node_t* phys_node = &vt->pt->physical_nodes[i];
        bw_throttle_reset(phys_node);
    }
}
