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
/**
 *  \file
 * 
 *  Constructs a virtual topology
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <numa.h>
#include "cpu/cpu.h"
#include "error.h"
#include "measure.h"
#include "topology.h"
#include "model.h"

#define MAX_NUM_MC_PCI_BUS 16

extern latency_model_t latency_model;

void rr_set_next_cpu_based_on_rank(int rank, int max_rank);
void partition_cpus_based_on_rank(int rank, int max_rank, int num_cpus,
                                  virtual_topology_t* virtual_topology);

int select_cpus_based_on_local_rank(virtual_topology_t* virtual_topology)
{
    int num_cpus = 0;
    int vnode;
    virtual_node_t* virtual_node;
    physical_node_t* physical_node;
    int n_procs = latency_model.max_local_processe_ranks;
    int rank = latency_model.process_local_rank;

    if (rank >= n_procs) {
        DBG_LOG(ERROR, "process rank %d exceeded limit of %d max emulated processes\n",
                       rank, n_procs);
        return E_ERROR;
    }

    for (vnode = 0; vnode < virtual_topology->num_virtual_nodes; ++vnode) {
        virtual_node = &virtual_topology->virtual_nodes[vnode];
        physical_node = virtual_node->dram_node;
        num_cpus += physical_node->num_cpus;
    }

    DBG_LOG(DEBUG, "number of cpus is %d\n", num_cpus);

    if (n_procs > (num_cpus/2)) {
        // do not partition CPUs, but bind this process to the CPU
        // indicated by our rank, after that, a new thread will be
        // bound to next available CPU on a round robin policy from
        // the max rank
        rr_set_next_cpu_based_on_rank(rank, n_procs);
    } else {
        // partition the CPUs to each rank
        // some CPUs may end up idle/without bound processes, if n_procs is not
        // multiple of 2
        // TODO: warn or avoid idle CPUs
        partition_cpus_based_on_rank(rank, n_procs, num_cpus, virtual_topology);
    }

    return E_SUCCESS;
}

/** 
 *  \brief Returns a list of memory-controller pci buses
 */
int get_mc_pci_bus_list(pci_regs_t *bus_id_list[], int max_list_size, int* dev_countp)
{
    FILE* fp;
    char  buf[2048];
    int   bus_id, dev_id, funct;
    int   last_bus_id = -1;
    int   channel = 0;
    char  dontcare[512];
    int   dev_count = 0;

    fp = popen("lspci", "r");
    if (fp == NULL) {
        return E_ERROR;
    }

    for (dev_count=0; fgets(buf, sizeof(buf)-1, fp) != NULL; ) {
        if (strstr(buf, "Thermal Control")) {
            if (sscanf(buf, "%x:%x.%x %s", &bus_id, &dev_id, &funct, dontcare) == 4) {
                if (bus_id != last_bus_id) {
                    ++dev_count;
                    last_bus_id = bus_id;

                    if (dev_count > max_list_size) {
                        pclose(fp);
                        return E_ERROR;
                    }
                    channel = 0;
                    bus_id_list[dev_count-1] = (pci_regs_t*)malloc(sizeof(pci_regs_t));
                }

                bus_id_list[dev_count-1]->addr[channel].bus_id = bus_id;
                bus_id_list[dev_count-1]->addr[channel].dev_id = dev_id;
                bus_id_list[dev_count-1]->addr[channel].funct = funct;
                ++channel;
                bus_id_list[dev_count-1]->channels = channel;
            }
        }
    }
    *dev_countp = dev_count;
    pclose(fp);

    return E_SUCCESS;
}


/**
 *  \brief Discovers the physical memory-controller pci bus topology of the 
 *  machine, which includes the socket each memory controller is attached to
 * 
 *  To discover where a memory controller is connected to, we throttle the rest of 
 *  the memory controllers and measure local bandwidth of each node. The unthrottled 
 *  memory controller is attached to the node with the highest local bandwidth
 */
int discover_mc_pci_topology(cpu_model_t* cpu_model, physical_node_t* physical_nodes[], int num_physical_nodes)
{
    pci_regs_t *regs_addr[16];
    int dev_count;
    physical_node_t* local_node = NULL;
    int b, i;
    double max_local_rbw;
    double rbw;
    int count = 0;
    uint16_t throttle_reg_val;

    get_mc_pci_bus_list(regs_addr, MAX_NUM_MC_PCI_BUS, &dev_count);

    if (dev_count < num_physical_nodes) {
        // TODO: application is terminated on error only if in DEBUG mode
        DBG_LOG(WARNING, "The number of physical nodes is greater than the number of memory-controller pci buses.\n");
    }

    for (b=0; b<dev_count; b++) {
        // throttle all other buses except the one we are currently trying 
        // to figure out where it is attached
        for (i=0; i<dev_count; i++) {
            if (i == b) {
                cpu_model->get_throttle_register(regs_addr[i], THROTTLE_DDR_ACT, &throttle_reg_val);
                if (throttle_reg_val < 0x8fff)
                    cpu_model->set_throttle_register(regs_addr[i], THROTTLE_DDR_ACT, 0x8fff);
            } else {
                cpu_model->set_throttle_register(regs_addr[i], THROTTLE_DDR_ACT, 0x800f);
            }
        }
        // measure local bandwidth of each node
        max_local_rbw = 0;
        for (i=0; i<num_physical_nodes; i++) {
            physical_node_t* node_i = physical_nodes[i];
            rbw = measure_read_bw(node_i->node_id, node_i->node_id);
            if (rbw > max_local_rbw) {
                max_local_rbw = rbw;
                local_node = node_i;
            }
        }
        if (local_node) {
            DBG_LOG(DEBUG, "setting node_id %d to bus %X\n", local_node->node_id, regs_addr[b]->addr[0].bus_id);
            local_node->mc_pci_regs = regs_addr[b];
            if (++count == num_physical_nodes) break;
        }
    }

    for (i=0; i<dev_count; i++) {
        cpu_model->get_throttle_register(regs_addr[i], THROTTLE_DDR_ACT, &throttle_reg_val);
        if (throttle_reg_val < 0x8fff)
            cpu_model->set_throttle_register(regs_addr[i], THROTTLE_DDR_ACT, 0x8fff);
    }

    return E_SUCCESS;
}

/** 
 * \brief Loads the memory controller pci topology from a file
 */
static int load_mc_pci_topology(const char* path, physical_node_t* physical_nodes[], int num_physical_nodes)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int j;
    int bus_id, dev_id, funct;
    int node_id;
    int dev_count;
    pci_regs_t *regs = NULL;
    int channel = 0;
    int last_bus_id = -1;

    fp = fopen(path, "r");
    if (fp == NULL) {
        return E_ERROR;
    }

    DBG_LOG(INFO, "Loading memory-controller pci topology from %s\n", path);
    for (dev_count = 0; (read = getline(&line, &len, fp)) != -1; ) {
        sscanf(line, "%d\t%x:%x.%x", &node_id, &bus_id, &dev_id, &funct);
        DBG_LOG(INFO, "node: %d, pci addr: %x:%x.%x\n", node_id, bus_id, dev_id, funct);
        if (bus_id != last_bus_id) {
            last_bus_id = bus_id;
            regs = (pci_regs_t*) malloc(sizeof(pci_regs_t));
            channel = 0;
            dev_count++;

            for (j=0; j<num_physical_nodes; j++) {
                if (node_id == physical_nodes[j]->node_id) {
                    physical_nodes[j]->mc_pci_regs = regs;
                    DBG_LOG(INFO, "node: %d, pci bus: 0x%x\n", physical_nodes[j]->node_id, bus_id);
                }
            }
        }

        regs->addr[channel].bus_id = bus_id; 
        regs->addr[channel].dev_id = dev_id; 
        regs->addr[channel].funct = funct;
        ++channel;
        regs->channels = channel;
    }
    free(line);
    if (dev_count < num_physical_nodes) {
        DBG_LOG(WARNING, "No complete memory-controller pci topology found in %s\n", path);
    }
    fclose(fp);
    return E_SUCCESS;
}


/** 
 * \brief Saves the memory controller pci topology in a file for later reuse
 */
static int save_mc_pci_topology(const char* path, physical_node_t* physical_nodes[], int num_physical_nodes)
{
    int i, j;
    FILE *fp;

    fp = fopen(path, "w");
    if (fp == NULL) {
        return E_ERROR;
    }

    DBG_LOG(INFO, "Saving memory-controller pci topology into %s\n", path);
    for (i=0; i<num_physical_nodes; i++) {
        pci_regs_t *regs = physical_nodes[i]->mc_pci_regs;
        int node_id = physical_nodes[i]->node_id;
        for (j=0; regs != NULL && j < regs->channels; ++j) {
            DBG_LOG(INFO, "node: %d, pci addr: %x:%x.%x\n", node_id, regs->addr[j].bus_id, regs->addr[j].dev_id, regs->addr[j].funct);
            fprintf(fp, "%d\t%x:%x.%x\n", node_id, regs->addr[j].bus_id, regs->addr[j].dev_id, regs->addr[j].funct);
        }
    }
    fclose(fp);
    return E_SUCCESS;
}

int num_cpus(struct bitmask* bitmask) 
{
    int i,n;
    // if we had knowledge of the bitmask structure then we could
    // count the bits faster but bitmask seems to be an opaque structure
    for (i=0, n=0; i<numa_num_configured_cpus(); i++) {
        if (numa_bitmask_isbitset(bitmask, i)) {
            n++;
        }
    }
    return n;
}

// number of cpus in the system
int system_num_cpus()
{
    return sysconf( _SC_NPROCESSORS_ONLN );
}

void print_bitmask(struct bitmask* bitmask) {
    int i;
    for (i=0; i<numa_num_configured_cpus(); i++) {
        if (numa_bitmask_isbitset(bitmask, i)) {
            DBG_LOG(INFO, "bit %d\n", i);
        }
    }
    return;
}

int next_cpu(struct bitmask* bitmask, int cpu_id)
{
    int i;
    // if we had knowledge of the bitmask structure then we could
    // count the bits faster but bitmask seems to be an opaque structure
    for (i=cpu_id; i<numa_num_configured_cpus(); i++) {
        if (numa_bitmask_isbitset(bitmask, i)) {
            return i;
        }
    }
    return -1;
}

int first_cpu(struct bitmask* bitmask)
{
    return next_cpu( bitmask, 0);
}

int partition_cpus(virtual_topology_t* virtual_topology)
{
    int ret = E_SUCCESS;
    // if there are more than one emulated process, then partition the available CPUs
    // among all processes, based on the current local rank
    if (latency_model.max_local_processe_ranks > 1) {
        ret = select_cpus_based_on_local_rank(virtual_topology);
    }

    return ret;
}

/**
 * \brief Construct a virtual topology
 *
 * Constructs a NUMA virtual topology where two physical sockets are fused into a 
 * single virtual node
 */
int init_virtual_topology(config_t* cfg, cpu_model_t* cpu_model, virtual_topology_t** virtual_topologyp)
{
    char* mc_pci_file;
    char* str;
    char* saveptr = NULL;
    char* token = "NULL";
    int* physical_node_ids;
    physical_node_t** physical_nodes = NULL;
    int num_physical_nodes;
    int n, v, i, j, sibling_idx;
    int node_id;
    physical_node_t* node_i, *node_j, *sibling_node;
    int ret;
    int min_distance;
    int hyperthreading;
    struct bitmask* mem_nodes;
    virtual_topology_t* virtual_topology;

    if (__cconfig_lookup_string(cfg, "topology.physical_nodes", &str) == CONFIG_FALSE) {
        return E_ERROR;
    }

    DBG_LOG(DEBUG, "Possible NUMA nodes are %d\n", numa_num_possible_nodes());
    DBG_LOG(DEBUG, "NUMA nodes allowed are %lu\n", numa_get_mems_allowed()->size);
    DBG_LOG(DEBUG, "NUMA configured CPUs are %d\n", numa_num_configured_cpus());

    // parse the physical nodes string
    physical_node_ids = calloc(numa_num_possible_nodes(), sizeof(*physical_node_ids));
    num_physical_nodes = 0;

    while ((token = strtok_r(str, ",", &saveptr))) {
        physical_node_ids[num_physical_nodes] = atoi(token);
        str = NULL;
        if (++num_physical_nodes > numa_num_possible_nodes()) {
            // we re being asked to run on more nodes than available
            free(physical_node_ids);
            ret = E_ERROR;
            goto done;
        }
    }
    if (!(physical_nodes = calloc(num_physical_nodes, sizeof(*physical_nodes)))) {
        DBG_LOG(ERROR, "Failed physical nodes allocation\n");
        abort();
    }

    // select those nodes we can run on (e.g. not constrained by any numactl)
    mem_nodes = numa_get_mems_allowed();
    for (i=0, n=0; i<num_physical_nodes; i++) {
        node_id = physical_node_ids[i];
        if (numa_bitmask_isbitset(mem_nodes, node_id)) {
            physical_nodes[n] = malloc(sizeof(**physical_nodes));
            memset(physical_nodes[n], 0, sizeof(**physical_nodes));
            physical_nodes[n]->node_id = node_id;
            physical_nodes[n]->cpu_bitmask = numa_allocate_cpumask();
            physical_nodes[n]->cpu_model = cpu_model;
            numa_node_to_cpus(node_id, physical_nodes[n]->cpu_bitmask);
            __cconfig_lookup_bool(cfg, "topology.hyperthreading", &hyperthreading);
            if (hyperthreading) {
                physical_nodes[n]->num_cpus = num_cpus(physical_nodes[n]->cpu_bitmask);
            } else {
                DBG_LOG(INFO, "Not using hyperthreading.\n");
                // disable the upper half of the processors in the bitmask
                physical_nodes[n]->num_cpus = num_cpus(physical_nodes[n]->cpu_bitmask) / 2;
                int fc = first_cpu(physical_nodes[n]->cpu_bitmask);
                for (j=fc+system_num_cpus()/2; j<fc+system_num_cpus()/2+physical_nodes[n]->num_cpus; j++) {
                    if (numa_bitmask_isbitset(physical_nodes[n]->cpu_bitmask, j)) {
                        numa_bitmask_clearbit(physical_nodes[n]->cpu_bitmask, j);
                    }
                }
            }
            DBG_LOG(INFO, "%d CPUs on physical node %d\n", physical_nodes[n]->num_cpus, n);
            n++;
        }
    }
    free(physical_node_ids);
    num_physical_nodes = n;

    // If pci bus topology of each physical node is not provided then discover it.
    // The bus topology must be always known even if BW model is disabled.
    if (__cconfig_lookup_string(cfg, "topology.mc_pci", &mc_pci_file) == CONFIG_FALSE ||
          (__cconfig_lookup_string(cfg, "topology.mc_pci", &mc_pci_file) == CONFIG_TRUE &&
          load_mc_pci_topology(mc_pci_file, physical_nodes, num_physical_nodes) != E_SUCCESS))
    {
        discover_mc_pci_topology(cpu_model, physical_nodes, num_physical_nodes);
        save_mc_pci_topology(mc_pci_file, physical_nodes, num_physical_nodes);
        DBG_LOG(INFO, "Topology MC PCI file saved, restart the process\n");
        exit(0);
    }

    // form virtual nodes by grouping physical nodes that are close to each other
    virtual_topology = malloc(sizeof(*virtual_topology));
    virtual_topology->num_virtual_nodes = num_physical_nodes / 2 + num_physical_nodes % 2;
    virtual_topology->virtual_nodes = calloc(virtual_topology->num_virtual_nodes, 
                                             sizeof(*(virtual_topology->virtual_nodes)));

    DBG_LOG(INFO, "Number of physical nodes %d\n", num_physical_nodes);
    DBG_LOG(INFO, "Number of virtual nodes %d\n", virtual_topology->num_virtual_nodes);

    for (i=0, v=0; i<num_physical_nodes; i++) {
        min_distance = INT_MAX;
        sibling_node = NULL;
        sibling_idx = -1;
        if ((node_i = physical_nodes[i]) == NULL) {
            continue;
        }

        for (j=i+1; j<num_physical_nodes; j++) {
            if ((node_j = physical_nodes[j]) == NULL) {
                continue;
            }
            // TODO: numa_distance() returns '0' on error
            if (numa_distance(node_i->node_id,node_j->node_id) < min_distance) {
                sibling_node = node_j;
                sibling_idx = j;
            }
        }

        if (sibling_node) {
            physical_nodes[i] = physical_nodes[sibling_idx] = NULL;
            virtual_node_t* virtual_node = &virtual_topology->virtual_nodes[v];
            virtual_node->dram_node = node_i;
            virtual_node->nvram_node = sibling_node;
            virtual_node->dram_node->latency = measure_latency(cpu_model,
                                                               virtual_node->dram_node->node_id,
                                                               virtual_node->dram_node->node_id);
            virtual_node->nvram_node->latency = measure_latency(cpu_model,
                                                                virtual_node->dram_node->node_id,
                                                                virtual_node->nvram_node->node_id);
            virtual_node->node_id = v;
            DBG_LOG(INFO, "Fusing physical nodes %d %d into virtual node %d\n", 
                    node_i->node_id, sibling_node->node_id, virtual_node->node_id);
            v++;
        }
    }

    // any physical node that is not paired with another physical node is 
    // formed into a virtual node on its own
    if (2*v < num_physical_nodes) {
        for (i=0; i<num_physical_nodes; i++) {
            node_i = physical_nodes[i];
            virtual_node_t* virtual_node = &virtual_topology->virtual_nodes[v];
            virtual_node->dram_node = virtual_node->nvram_node = node_i;
            virtual_node->node_id = v;
            virtual_node->dram_node->latency = measure_latency(cpu_model,
                                                               virtual_node->dram_node->node_id,
                                                               virtual_node->dram_node->node_id);
            DBG_LOG(WARNING, "Forming physical node %d into virtual node %d without a sibling node.\n",
                    node_i->node_id, virtual_node->node_id);
        }
    }

    *virtual_topologyp = virtual_topology;
    ret = E_SUCCESS;

done:
    free(physical_nodes);
    return ret;
}
