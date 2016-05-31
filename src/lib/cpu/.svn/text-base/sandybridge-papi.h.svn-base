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
#ifndef __CPU_SANDYBRIDGE_H
#define __CPU_SANDYBRIDGE_H

#include <papi.h>
#include <math.h>
#include "debug.h"

// Perfmon2 is a library that provides a generic interface to access the PMU. It also comes with
// applications to list all available performance events with their architecutre specific 
// detailed description and translate them to their respective event code. showevtinfo application can 
// be used to list all available performance event names with detailed desciption and check_events application
// can be used to translate the performance event to the corresponding event code.  

// These events will be initialized and started.
// Every event reading will return an array with the values for all these events.
// The array index is the same index used to define the event in the *_native_events array below
const char *sandybridge_native_events[MAX_NUM_EVENTS] = {
    "CYCLE_ACTIVITY:STALLS_L2_PENDING",
    "MEM_LOAD_UOPS_MISC_RETIRED:LLC_MISS",
    "MEM_LOAD_UOPS_RETIRED:L3_HIT",
    NULL
};


void sandybridge_latency_calibration_local(int *hw_latency, int target_latency) {
	if ((*hw_latency + 10) < target_latency)
		*hw_latency += 10;
}

void sandybridge_latency_calibration_remote(int *hw_latency, int target_latency) {
	if ((*hw_latency + 30) < target_latency)
		*hw_latency += 30;
}

uint64_t sandybridge_read_stall_events_local() {
    long long values[MAX_NUM_EVENTS];
    uint64_t events = 0;

    if (pmc_events_read_local_thread(values) == PAPI_OK) {
        uint64_t cycle_activity_stalls_l2_pending_diff = values[0];
        uint64_t mem_load_uops_misc_retired_llc_miss_diff = values[1];
        uint64_t mem_load_uops_retired_l3_hit_diff = values[2];

        DBG_LOG(DEBUG, "read stall L2 cycles %lu, LLC miss %lu, L3 hit %lu\n",
        		cycle_activity_stalls_l2_pending_diff, mem_load_uops_misc_retired_llc_miss_diff,
        		mem_load_uops_retired_l3_hit_diff);

    	uint64_t uden = 7.0 * mem_load_uops_misc_retired_llc_miss_diff + mem_load_uops_retired_l3_hit_diff;
        if (uden == 0) {
            return 0;
        }
        double den = uden;
        double num = 7.0 * mem_load_uops_misc_retired_llc_miss_diff;

        events = (uint64_t) floorl(cycle_activity_stalls_l2_pending_diff*num/den);
    } else {
        DBG_LOG(DEBUG, "read stall cycles failed\n");
    }

    return events;
}

#endif /* __CPU_SANDYBRIDGE_H */
