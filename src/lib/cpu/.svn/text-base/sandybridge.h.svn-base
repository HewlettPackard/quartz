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

#include <math.h>
#include "thread.h"
#include "cpu/pmc.h"
#include "debug.h"

// Perfmon2 is a library that provides a generic interface to access the PMU. It also comes with
// applications to list all available performance events with their architecutre specific 
// detailed description and translate them to their respective event code. showevtinfo application can 
// be used to list all available performance event names with detailed desciption and check_events application
// can be used to translate the performance event to the corresponding event code.  

#undef FOREACH_PMC_HW_EVENT
#define FOREACH_PMC_HW_EVENT(ACTION)                                                                       \
  ACTION("CYCLE_ACTIVITY:STALLS_L2_PENDING", NULL, 0x55305a3)                                              \
  ACTION("MEM_LOAD_UOPS_MISC_RETIRED:LLC_MISS", NULL, 0x5302d4)                                            \
  ACTION("MEM_LOAD_UOPS_RETIRED:L3_HIT", NULL, 0x5304d1)                                                   \
  ACTION("INSTRUCTION_RETIRED", NULL, 0x5300c0)               

#undef FOREACH_PMC_EVENT
#define FOREACH_PMC_EVENT(ACTION, prefix)                                                                  \
  ACTION(ldm_stall_cycles, prefix)


DECLARE_ENABLE_PMC(sandybridge, ldm_stall_cycles)
{
    ASSIGN_PMC_HW_EVENT_TO_ME("CYCLE_ACTIVITY:STALLS_L2_PENDING", 0);
    ASSIGN_PMC_HW_EVENT_TO_ME("MEM_LOAD_UOPS_MISC_RETIRED:LLC_MISS", 1);
    //ASSIGN_PMC_HW_EVENT_TO_ME("INSTRUCTION_RETIRED", 2);
    ASSIGN_PMC_HW_EVENT_TO_ME("MEM_LOAD_UOPS_RETIRED:L3_HIT", 2);

    return E_SUCCESS;
}

DECLARE_CLEAR_PMC(sandybridge, ldm_stall_cycles)
{
}

DECLARE_READ_PMC(sandybridge, ldm_stall_cycles)
{
	//return 0;
   uint64_t cycle_activity_stalls_l2_pending_diff = READ_MY_HW_EVENT_DIFF(0);
   uint64_t mem_load_uops_misc_retired_llc_miss_diff = READ_MY_HW_EVENT_DIFF(1);
   uint64_t mem_load_uops_retired_l3_hit_diff = READ_MY_HW_EVENT_DIFF(2);

   //return floor(cycle_activity_stalls_l2_pending_diff * (((double) (7*mem_load_uops_misc_retired_llc_miss_diff))/((double)(7*mem_load_uops_misc_retired_llc_miss_diff + mem_load_uops_retired_l3_hit_diff))));
   uint64_t uden = 7.0 * mem_load_uops_misc_retired_llc_miss_diff + mem_load_uops_retired_l3_hit_diff;
   if (uden == 0) {
      return 0;  
   }
   double den = uden;
   double num = 7.0 * mem_load_uops_misc_retired_llc_miss_diff;

   return (uint64_t) floorl(cycle_activity_stalls_l2_pending_diff*num/den);
}


PMC_EVENTS(sandybridge, 4)
#endif /* __CPU_SANDYBRIDGE_H */
