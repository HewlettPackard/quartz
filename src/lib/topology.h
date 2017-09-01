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

#ifndef __TOPOLOGY_H
#define __TOPOLOGY_H

#include <numa.h>
#include <uthash.h>
#include <utlist.h>

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
 
struct physical_node_s {
    int node_id;
    cpu_model_t* cpu_model;
    pci_regs_t* mc_pci_regs;

    /** number of node's physical CPUs */
    int num_cpus; 

    /** a bitmask of the node's physical CPUs */
    struct bitmask* cpu_bitmask; 

    /** number of virtual elements referencing this physical node */
    int num_vrefs;

    /** local memory latency */
    int local_latency; 

    /** all (local and remote) memory latencies */
    int* latencies; 

    /** local memory bandwidth throttle values */
    struct bw_throttle_s* bw_throttle; 
};

typedef struct physical_topology_s {
    struct physical_node_s* physical_nodes; // pointer to an array of physical nodes
    int num_nodes;
} physical_topology_t;

struct virtual_nvm_s {
    const char* name;
    int nvm_id;
    size_t size;
    int membind;
    const char* mountpath;
    struct physical_node_s* phys_node;
};

typedef struct virtual_node_s {
    const char* name;
    int node_id;
    int cpunodebind;
    int membind;
    struct physical_node_s* dram_node;
    struct virtual_nvm_s* nvm;

    /** a bitmask of the node's physical CPUs */
    struct bitmask* cpumask;

    /** 
     * id relative to other sibling nodes bound to the same underlying 
     * physical cpu node 
     */
    int sibling_id;

} virtual_node_t;

struct virtual_topology_element_s {
    const char* name;
    int id;
    void* element;

    /** Number of elements this element depends on */
    int dep_count; 

    /** Array of elements this element depends on */
    struct virtual_topology_element_s** dep; 

    config_setting_t* cfg;

    /** Virtual topology */
    struct virtual_topology_s* vt;

    UT_hash_handle hh;
};

struct virtual_topology_s {
    /** Physical topology this virtual topology builds on */
    struct physical_topology_s* pt;

    /** Number of elements */
    int num_elements;

    /** Elements hash table indexed by element name */
    struct virtual_topology_element_s* elements_ht; 

    /** Elements array table indexed by element id */
    struct virtual_topology_element_s** elements_ar;
};

struct virtual_node_iterator_s {
    int i;
    struct virtual_topology_s* vt;
};

struct virtual_nvm_iterator_s {
    int i;
    struct virtual_topology_s* vt;
};

typedef struct physical_node_s physical_node_t;
typedef struct virtual_nvm_s virtual_nvm_t;
typedef struct virtual_topology_element_s virtual_topology_element_t;
typedef struct virtual_topology_s virtual_topology_t;
typedef struct virtual_node_iterator_s virtual_node_iterator_t;
typedef struct virtual_nvm_iterator_s virtual_nvm_iterator_t;

int discover_physical_topology(cpu_model_t* cpu_model, physical_topology_t** physical_topology);
int physical_topology_from_xml(cpu_model_t* cpu_model, const char* xml_path, physical_topology_t** physical_topology);
int physical_topology_to_xml(physical_topology_t* physical_topology, const char* xml_path);
int load_physical_topology(config_t* cfg, physical_topology_t** physical_topology);
int discover_and_save_physical_topology(const char* filename);
int num_cpus_node(int node);
int system_num_cpus();
int num_cores_per_node();
int first_cpu(struct bitmask* bitmask);
int next_cpu(struct bitmask* bitmask, int cpu_id);
int virtual_cpu_id_to_phys_cpu_id(virtual_node_t* vnode, int cpu_id);

int create_virtual_topology(config_t* cfg, physical_topology_t* pt, virtual_topology_t** vtp);
int destroy_virtual_topology(virtual_topology_t* vt);
void crawl_virtual_topology(
    virtual_topology_t* vt, 
    void (*node_cb)(virtual_topology_element_t* vte, void* arg), void* node_cb_arg,
    void (*nvm_cb)(virtual_topology_element_t* vte, void* arg), void* nvm_cb_arg);
void visualize_virtual_topology(FILE* stream, virtual_topology_t* vt);

virtual_node_t* virtual_node(virtual_topology_t* vt, int node_id);

virtual_node_t* virtual_node_iterator_first(virtual_node_iterator_t* it, virtual_topology_t* vt);
virtual_node_t* virtual_node_iterator_next(virtual_node_iterator_t* it);
int virtual_node_iterator_done(virtual_node_iterator_t* it);

virtual_nvm_t* virtual_nvm_iterator_first(virtual_nvm_iterator_t* it, virtual_topology_t* vt);
virtual_nvm_t* virtual_nvm_iterator_next(virtual_nvm_iterator_t* it);
int virtual_nvm_iterator_done(virtual_nvm_iterator_t* it);

int bind_process_on_virtual_node(virtual_topology_t* vt, int virtual_node_id);
int bind_thread_on_virtual_cpu(virtual_topology_t* vt, pid_t tid, int virtual_node_id, int virtual_cpu_id);

#endif /* __TOPOLOGY_H */
