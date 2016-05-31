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

#include <math.h>
#include "thread.h"
#include "cpu/pmc.h"
#include "debug.h"

// Perfmon2 is a library that provides a generic interface to access the PMU. It also comes with
// applications to list all available performance events with their architecture specific
// detailed description and translate them to their respective event code. 'showevtinfo' application can
// be used to list all available performance event names with detailed description and 'check_events' application
// can be used to translate the performance event to the corresponding event code.  

extern __thread int tls_hw_local_latency;
extern __thread int tls_hw_remote_latency;
#ifdef MEMLAT_SUPPORT
extern __thread uint64_t tls_global_remote_dram;
extern __thread uint64_t tls_global_local_dram;
#endif

#undef FOREACH_PMC_HW_EVENT
#define FOREACH_PMC_HW_EVENT(ACTION)                                                                       \
  ACTION("CYCLE_ACTIVITY:STALLS_L2_PENDING", NULL, 0x55305a3)                                              \
  ACTION("MEM_LOAD_UOPS_L3_HIT_RETIRED:XSNP_NONE", NULL, 0x5308d2)                                        \
  ACTION("MEM_LOAD_UOPS_L3_MISS_RETIRED:REMOTE_DRAM", NULL, 0x530cd3)                                     \
  ACTION("MEM_LOAD_UOPS_L3_MISS_RETIRED:LOCAL_DRAM", NULL, 0x5303d3)

#undef FOREACH_PMC_EVENT
#define FOREACH_PMC_EVENT(ACTION, prefix)                                                                  \
  ACTION(ldm_stall_cycles, prefix)                                                                         \
  ACTION(remote_dram, prefix)

#define L3_FACTOR 7.0

DECLARE_ENABLE_PMC(haswell, ldm_stall_cycles)
{
    ASSIGN_PMC_HW_EVENT_TO_ME("CYCLE_ACTIVITY:STALLS_L2_PENDING", 0);
    ASSIGN_PMC_HW_EVENT_TO_ME("MEM_LOAD_UOPS_L3_HIT_RETIRED:XSNP_NONE", 1);
    ASSIGN_PMC_HW_EVENT_TO_ME("MEM_LOAD_UOPS_L3_MISS_RETIRED:REMOTE_DRAM", 2);
    ASSIGN_PMC_HW_EVENT_TO_ME("MEM_LOAD_UOPS_L3_MISS_RETIRED:LOCAL_DRAM", 3);

    return E_SUCCESS;
}

DECLARE_CLEAR_PMC(haswell, ldm_stall_cycles)
{
}

DECLARE_READ_PMC(haswell, ldm_stall_cycles)
{
   uint64_t l2_pending_diff  = READ_MY_HW_EVENT_DIFF(0);
   uint64_t llc_hit_diff     = READ_MY_HW_EVENT_DIFF(1);
   uint64_t remote_dram_diff = READ_MY_HW_EVENT_DIFF(2);
   uint64_t local_dram_diff  = READ_MY_HW_EVENT_DIFF(3);

   DBG_LOG(DEBUG, "read stall L2 cycles diff %lu; llc_hit %lu; cycles diff remote_dram %lu; local_dram %lu\n",
		   l2_pending_diff, llc_hit_diff, remote_dram_diff, local_dram_diff);

   if ((remote_dram_diff == 0) && (local_dram_diff == 0)) return 0;
#ifdef MEMLAT_SUPPORT
   tls_global_local_dram += local_dram_diff;
#endif

   // calculate stalls based on L2 stalls and LLC miss/hit
   double num = L3_FACTOR * (remote_dram_diff + local_dram_diff);
   double den = num + llc_hit_diff;
   if (den == 0) return 0;
   return (uint64_t) ((double)l2_pending_diff * (num / den));
}


DECLARE_ENABLE_PMC(haswell, remote_dram)
{
    ASSIGN_PMC_HW_EVENT_TO_ME("CYCLE_ACTIVITY:STALLS_L2_PENDING", 0);
    ASSIGN_PMC_HW_EVENT_TO_ME("MEM_LOAD_UOPS_L3_HIT_RETIRED:XSNP_NONE", 1);
    ASSIGN_PMC_HW_EVENT_TO_ME("MEM_LOAD_UOPS_L3_MISS_RETIRED:REMOTE_DRAM", 2);
    ASSIGN_PMC_HW_EVENT_TO_ME("MEM_LOAD_UOPS_L3_MISS_RETIRED:LOCAL_DRAM", 3);

    return E_SUCCESS;
}

DECLARE_CLEAR_PMC(haswell, remote_dram)
{
}

DECLARE_READ_PMC(haswell, remote_dram)
{
   uint64_t l2_pending_diff  = READ_MY_HW_EVENT_DIFF(0);
   uint64_t llc_hit_diff     = READ_MY_HW_EVENT_DIFF(1);
   uint64_t remote_dram_diff = READ_MY_HW_EVENT_DIFF(2);
   uint64_t local_dram_diff  = READ_MY_HW_EVENT_DIFF(3);

   DBG_LOG(DEBUG, "read stall L2 cycles diff %lu; llc_hit %lu; cycles diff remote_dram %lu; local_dram %lu\n",
		   l2_pending_diff, llc_hit_diff, remote_dram_diff, local_dram_diff);

   if ((remote_dram_diff == 0) && (local_dram_diff == 0)) return 0;
#ifdef MEMLAT_SUPPORT
   tls_global_remote_dram += remote_dram_diff;
#endif

   // calculate stalls based on L2 stalls and LLC miss/hit
   double num = L3_FACTOR * (remote_dram_diff + local_dram_diff);
   double den = num + llc_hit_diff;
   if (den == 0) return 0;
   double stalls = (double)l2_pending_diff * (num / den);

   // calculate remote dram stalls based on total stalls and local/remote dram accesses
   // also consider the weight of remote memory access against local memory access
   den = (remote_dram_diff * tls_hw_remote_latency) + (local_dram_diff * tls_hw_local_latency);
   if (den == 0) return 0;
   return (uint64_t) (stalls * ((double)(remote_dram_diff * tls_hw_remote_latency) / den));
}


PMC_EVENTS(haswell, 4)
#endif /* __CPU_HASWELL_H */
