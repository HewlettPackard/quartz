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
#include "dev.h"

#ifdef PAPI_SUPPORT
#include "sandybridge-papi.h"
#include "ivybridge-papi.h"
#include "haswell-papi.h"
#else
#include "sandybridge.h"
#include "ivybridge.h"
#include "haswell.h"
#endif

int intel_xeon_ex_set_throttle_register(pci_regs_t *regs, throttle_type_t throttle_type, uint16_t val)
{
    int offset;
    int i;

    switch(throttle_type) {
        case THROTTLE_DDR_ACT:
            offset = 0x190; break;
        case THROTTLE_DDR_READ:
            offset = 0x192; break;
        case THROTTLE_DDR_WRITE:
            offset = 0x194; break;
        default:
            offset = 0x190;
    }

    // write to all 4 channels

    // first Activate throttling
    /*set_pci(bus_id, 0x10, 0x0, 0x190, (uint16_t) val);
    set_pci(bus_id, 0x10, 0x1, 0x190, (uint16_t) val);
    set_pci(bus_id, 0x10, 0x4, 0x190, (uint16_t) val);
    set_pci(bus_id, 0x10, 0x5, 0x190, (uint16_t) val);*/

    // then the Read or Write throttling
    for (i=0; i < regs->channels; ++i) {
        set_pci(regs->addr[i].bus_id, regs->addr[i].dev_id, regs->addr[i].funct, offset, (uint16_t) val);
    }

    return 0;
}

int intel_xeon_ex_get_throttle_register(pci_regs_t *regs, throttle_type_t throttle_type, uint16_t* val)
{
    int offset;

    switch(throttle_type) {
        case THROTTLE_DDR_ACT:
            offset = 0x190; break;
        case THROTTLE_DDR_READ:
            offset = 0x192; break;
        case THROTTLE_DDR_WRITE:
            offset = 0x194; break;
        default:
            offset = 0x190;
    }

    // read just channel 1
    get_pci(regs->addr[0].bus_id, regs->addr[0].dev_id, regs->addr[0].funct, offset, val);
    return 0;
}


// desc is fixed in cpu_model() if not Xeon

cpu_model_t cpu_model_intel_xeon_ex = {
    .microarch = SandyBridgeXeon,
#ifdef PAPI_SUPPORT
    .pmc_events = {sandybridge_native_events, sandybridge_read_stall_events_local, NULL},
#else
    .pmc_events = PMC_EVENTS_PTR(sandybridge),
#endif
    .set_throttle_register = intel_xeon_ex_set_throttle_register,
    .get_throttle_register = intel_xeon_ex_get_throttle_register
};

cpu_model_t cpu_model_intel_xeon_ex_v2 = {
    .microarch = IvyBridgeXeon,
#ifdef PAPI_SUPPORT
    .pmc_events = {ivybridge_native_events, ivybridge_read_stall_events_local, ivybridge_read_stall_events_remote},
#else
    .pmc_events = PMC_EVENTS_PTR(ivybridge),
#endif
    .set_throttle_register = intel_xeon_ex_set_throttle_register,
    .get_throttle_register = intel_xeon_ex_get_throttle_register
};

cpu_model_t cpu_model_intel_xeon_ex_v3 = {
    .microarch = HaswellXeon,
#ifdef PAPI_SUPPORT
    .pmc_events = {haswell_native_events, haswell_read_stall_events_local, haswell_read_stall_events_remote},
#else
    .pmc_events = PMC_EVENTS_PTR(haswell),
#endif
    .set_throttle_register = intel_xeon_ex_set_throttle_register,
    .get_throttle_register = intel_xeon_ex_get_throttle_register
};
