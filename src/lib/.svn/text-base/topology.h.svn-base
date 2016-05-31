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
#ifndef __TOPOLOGY_H
#define __TOPOLOGY_H

#include <numa.h>
#include "config.h"
#include "cpu/cpu.h"
#include "dev.h"

/* DOXYGEN Documentation : */

/**
    \page virtual_topology Virtual topology
 
    The emulator constructs a topology of virtual nodes out of physical nodes
    (i.e., NUMA sockets) that represents the arrangement of processors, DRAM, 
    and NVRAM of the virtual machine that the emulator emulates. 

    Currently, the emulator supports a NUMA virtual topology where essentially
    two physical sockets are fused into a single virtual node. Each virtual 
    node comprises the processors from one socket only (active socket), and 
    DRAM from both two physical sockets. The DRAM attached to the active socket
    is used as the virtual node's locally attached DRAM and the DRAM of the other 
    socket (passive) is used as the virtual node's locally attached NVRAM.
    This topology allows us to emulate a machine that has both DRAM and NVRAM but
    reduces the computation capacity of the machine to half.
    
    In the future we would like to support a topology that matches the shared NVRAM
    storage of The Machine.

 */
 


typedef struct {
    int node_id;
    cpu_model_t* cpu_model;
    pci_regs_t  *mc_pci_regs;
    int num_cpus; // number of node's cpus
    struct bitmask* cpu_bitmask; // a bitmask of the node's CPUs 

    // this is actual physical latency. the latency number though depends on 
    // whether the node corresponds to a dram node or a nvram node. 
    // if dram then latency is the measured local latency to dram.
    // if nvram then latency is the measured remote latency to the sibling nvram node
    int latency; 
} physical_node_t;

typedef struct virtual_node_s {
    int node_id;
    physical_node_t* dram_node;
    physical_node_t* nvram_node;
    //cpu_model_t* cpu_model;
} virtual_node_t;

typedef struct virtual_topology_s {
    virtual_node_t* virtual_nodes; // pointer to an array of virtual nodes
    int num_virtual_nodes;
} virtual_topology_t;

int init_virtual_topology(config_t* cfg, cpu_model_t* cpu_model, virtual_topology_t** virtual_topologyp);
int system_num_cpus();
int first_cpu(struct bitmask* bitmask);
int next_cpu(struct bitmask* bitmask, int cpu_id);

#endif /* __TOPOLOGY_H */
