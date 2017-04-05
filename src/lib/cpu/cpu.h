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
#ifndef __CPU_H
#define __CPU_H

#include <stddef.h>
#include <stdint.h>
#include "dev.h"

#define MAX_THROTTLE_VALUE 1023

int set_throttle_register(int node, uint64_t val);
size_t cpu_llc_size_bytes();

struct pmc_set_s;

typedef enum {
    THROTTLE_DDR_ACT = 0,
    THROTTLE_DDR_READ,
    THROTTLE_DDR_WRITE
} throttle_type_t;

// order matters. see cpu_model()
typedef enum {
    Invalid,
    SandyBridge,
    SandyBridgeXeon,
    IvyBridge,
    IvyBridgeXeon,
    Haswell,
    HaswellXeon
} microarch_t;

typedef struct
{
    int family;
    int model;
    microarch_t microarch;
} microarch_ID_t;

/**
 *  CPU object that encapsulates processor-specific methods for accessing
 *  performance counters and memory controller PCI registers
 */
typedef struct cpu_model_s {
    microarch_t microarch; // processor description
    size_t llc_size_bytes; // last level cache size
//    int speed_mhz; // cpu clock frequency
    struct pmc_events_s* pmc_events; // performance monitoring events supported by the processor
    int (*set_throttle_register)(pci_regs_t *regs, throttle_type_t throttle_type, uint16_t val);
    int (*get_throttle_register)(pci_regs_t *regs, throttle_type_t throttle_type, uint16_t* val);
} cpu_model_t;

cpu_model_t* cpu_model();
int cpu_speed_mhz();

#endif /* __CPU_H */
