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
#ifndef __CPU_HASWELL_H
#define __CPU_HASWELL_H

#include <papi.h>
#include "debug.h"

// Perfmon2 is a library that provides a generic interface to access the PMU. It also comes with
// applications to list all available performance events with their architecture specific
// detailed description and translate them to their respective event code. 'showevtinfo' application can
// be used to list all available performance event names with detailed description and 'check_events' application
// can be used to translate the performance event to the corresponding event code.  

// These events will be initialized and started.
// Every event reading will return an array with the values for all these events.
// The array index is the same index used to define the event in the *_native_events array below
const char *haswell_native_events[MAX_NUM_EVENTS] = {
    "CYCLE_ACTIVITY:STALLS_L2_PENDING",
    "MEM_LOAD_UOPS_L3_HIT_RETIRED:XSNP_NONE",
    "MEM_LOAD_UOPS_L3_MISS_RETIRED:REMOTE_DRAM",
    "MEM_LOAD_UOPS_L3_MISS_RETIRED:LOCAL_DRAM"
};

uint64_t haswell_read_stall_events_local() {
    long long values[MAX_NUM_EVENTS];
    uint64_t events = 0;

    if (pmc_events_read_local_thread(values) == PAPI_OK) {
		uint64_t l2_pending = values[0];
		uint64_t llc_hit  = values[1];
		uint64_t remote_dram = values[2];
		uint64_t local_dram  = values[3];

		DBG_LOG(DEBUG, "read stall L2 cycles %lu; llc_hit %lu; remote_dram %lu; local_dram %lu\n",
			l2_pending, llc_hit, remote_dram, local_dram);

		double num = remote_dram + local_dram;
		double den = num + llc_hit;
		if (den == 0) return 0;

		events = (uint64_t)((double)l2_pending * ((double)num / den));
    } else {
        DBG_LOG(ERROR, "read stall cycles failed\n");
    }

    return events;
}

uint64_t haswell_read_stall_events_remote() {
    long long values[MAX_NUM_EVENTS];
    uint64_t events = 0;

    if (pmc_events_read_local_thread(values) == PAPI_OK) {
		uint64_t l2_pending = values[0];
		uint64_t llc_hit  = values[1];
		uint64_t remote_dram = values[2];
		uint64_t local_dram  = values[3];

		DBG_LOG(DEBUG, "read stall L2 cycles %lu; llc_hit %lu; remote_dram %lu; local_dram %lu\n",
			l2_pending, llc_hit, remote_dram, local_dram);

		// calculate stalls based on l2 stalls and LLC miss/hit
		double num = remote_dram + local_dram;
		double den = num + llc_hit;
		if (den == 0) return 0;
		double stalls = (double)l2_pending * ((double)num / den);

		// calculate remote dram stalls based on total stalls and local/remote dram accesses
		den = remote_dram + local_dram;
		if (den == 0) return 0;
		events = (uint64_t) (stalls * ((double)remote_dram / den));
    } else {
        DBG_LOG(ERROR, "read stall cycles failed\n");
    }

    return events;
}

#endif /* __CPU_HASWELL_H */
