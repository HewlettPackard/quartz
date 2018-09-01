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
#include <stdlib.h>
#include "cpu/pmc.h"
#include "dev.h"
#include "error.h"
#include "thread.h"
#include "topology.h"

#pragma GCC push_options
#pragma GCC optimize ("O0")

// The width of general purpose counters are 40bits.
// https://www.felixcloutier.com/x86/RDPMC.html
#define RDPMC_MAX_VALUE 0xFFFFFFFFFF  

long long rdpmc(int counter) 
{

	unsigned eax;
	unsigned edx;
	unsigned long long r;

	__asm__ __volatile__ ("mov %2, %%ecx\n\t"
	                      "rdpmc\n\t"
	                      "mov %%eax, %0\n\t"
	                      "and $255, %%edx\n\t"
	                      "mov %%edx, %1\n\t"
	                      : "=m" (eax), "=m" (edx), "=m" (counter)
	                      : /* no inputs */
	                      : "eax", "ecx", "edx"); /* eax, ecx, edx clobbered */
	                      r = ((unsigned long long) edx << 32) | eax;
	return r;

}

int rdpmc32(int counter) {

	unsigned eax;
	
	__asm__ __volatile__ ("mov %1, %%ecx\n\t"
	                      "rdpmc\n\t"
	                      "mov %%eax, %0\n\t"
	                      : "=m" (eax), "=m" (counter)
	                      : /* no inputs */
	                      : "eax", "ecx", "edx"); /* eax, ecx, edx clobbered */
	return eax;

}
#pragma GCC pop_options


/*int num_used_hw_cntrs(pmc_events_t* events)
{
    int i;
    int used;
    pmc_hw_event_t* event = 0;

     // check if this a known registered hardware event
    for (i=0, used=0; events->known_hw_events[i].name; i++) {
        event = &events->known_hw_events[i];
        used += event->active ? 0 : 1;
    }
    return used;    
}*/

int get_avail_hw_cntr_id(pmc_events_t* events)
{
    int i;
    int used;
    pmc_hw_event_t* event = 0;
    int status = -1;

    int* hw_cntr_id_status = calloc(events->num_avail_hw_cntrs, sizeof(int));
    
    for (i=0, used=0; events->known_hw_events[i].name; i++) {
        event = &events->known_hw_events[i];
        if (event->active) {
            used++;
            hw_cntr_id_status[event->hw_cntr_id] = 1;
        }
    }
    
    if (used == events->num_avail_hw_cntrs) {
        goto done;
    }

    for (i=0; events->num_avail_hw_cntrs; i++) {
        if (hw_cntr_id_status[i] == 0) {
            status = i;
            goto done;
        }
    }

done:
	free(hw_cntr_id_status);
	return status;
}

pmc_hw_event_t* enable_pmc_hw_event(pmc_events_t* events, const char* name)
{
    int i;
    pmc_hw_event_t* event = 0;
    int found = 0;

     // check if this a known registered hardware event
    for (i=0; events->known_hw_events[i].name; i++) {
        event = &events->known_hw_events[i];
        if (strcasecmp(event->name, name) == 0) {
        	found = 1;
            if (event->active) {
                return event;
            }
            break;
        }
    }

    if (!found) {
        DBG_LOG(WARNING, "Unknown hardware performance monitoring event\n");
        return NULL;
    }

    // enable it 
    // need to find an available performance counter to monitor this event
    if ((event->hw_cntr_id = get_avail_hw_cntr_id(events)) < 0) {
        DBG_LOG(ERROR, "No available hardware performance counters\n");
        return NULL;
    }

    // assign an array to keep per processor last read values (useful to calculate the diff since the last read)
    int num_cpus = system_num_cpus();
    if (!event->last_val) {
        event->last_val = calloc(num_cpus, sizeof(*event->last_val));
    }
    for (i=0; i<num_cpus; i++) {
        event->last_val[i] = 0;
    }
    // call into the kernel driver to enable the counter on all processors
    if (set_counter(event->hw_cntr_id, event->encoding) != E_SUCCESS) {
    	DBG_LOG(ERROR, "Can't enable counter on all processors\n");
    	return NULL;
    }

    event->active = 1;
    return event;
}

void disable_pmc_hw_event(pmc_events_t* events, const char* name)
{
    int i;
    pmc_hw_event_t* event = 0;
    int found = 0;

    // check if this a known registered hardware event
    for (i=0; events->known_hw_events[i].name; i++) {
        event = &events->known_hw_events[i];
        if (strcasecmp(event->name, name) == 0) {
        	found = 1;
            if (!event->active) {
                return;
            }
            break;
        }
    }

    if (!found) {
        DBG_LOG(WARNING, "Unknown hardware performance monitoring event\n");
        return;
    }

    event->active = 0;
}

void clear_pmc_hw_event(pmc_hw_event_t* event)
{
    DBG_LOG(CRITICAL, "Unimplemented functionality\n");
}

uint64_t read_pmc_hw_event_cur(pmc_hw_event_t* event)
{
    return rdpmc(event->hw_cntr_id);
}

uint64_t read_pmc_hw_event_diff(pmc_hw_event_t* event)
{
    int cpu_id = thread_self()->cpu_id;
    uint64_t cur_val = read_pmc_hw_event_cur(event);
    uint64_t last_val = event->last_val[cpu_id];
    //if (cur_val < last_val && (event->hw_cntr_id == 0)) {
    if (cur_val < last_val) {
        event->last_val[cpu_id] = cur_val;
        return (cur_val + (RDPMC_MAX_VALUE - last_val));
    }
    event->last_val[cpu_id] = cur_val;
    return cur_val - last_val;
}


pmc_event_t* enable_pmc_event(cpu_model_t* cpu, const char* name) 
{
    int i;
    pmc_event_t* event = 0;
    int found = 0;

    // check if this a known registered event
    for (i=0; cpu->pmc_events->known_events[i].name; i++) {
        event = &cpu->pmc_events->known_events[i];
        if (strcasecmp(event->name, name) == 0) {
        	found = 1;
            if (event->active) {
                return event;
            }
            break;
        }
    }

    if (!found) {
    	return NULL;
    }

    // enable it 
    event->hw_events = NULL;
    event->num_hw_events = 0;
    if (event->enable(cpu->pmc_events, event) != E_SUCCESS) {
        assert(0 && "DIE");
        return NULL;
    }
    event->active = 1;
    return event;
}

int assign_pmc_hw_event_to_event(pmc_events_t* events, const char* name, pmc_event_t* event, int local_id)
{
    pmc_hw_event_t* hw_event;

    if (!(hw_event = enable_pmc_hw_event(events, name))) {
        return E_ERROR;
    }
    if (local_id != event->num_hw_events) {
        DBG_LOG(CRITICAL, "local_id does not match assign id\n")
        // TODO: application should abort here, look for all DBG_LOG(CRITICAL)
    }

    event->hw_events = realloc(event->hw_events, (event->num_hw_events+1) * sizeof(*event->hw_events));
    event->hw_events[event->num_hw_events] = hw_event;
    event->num_hw_events++; 
    return E_SUCCESS;
}

void release_all_pmc_hw_events_of_event(pmc_event_t* event)
{
    int i;
    if (event->num_hw_events > 0) {
        for (i=0; i<event->num_hw_events; i++) {
            event->hw_events[i]->active = 0;
        }
        free(event->hw_events);
        event->hw_events = NULL;
        event->num_hw_events = 0;
    }
}

void disable_pmc_event(cpu_model_t* cpu, const char* name) 
{
    int i;
    pmc_event_t* event;

    for (i=0; cpu->pmc_events->known_events[i].name; i++) {
        event = &cpu->pmc_events->known_events[i];
        if (strcasecmp(event->name, name) == 0 && event->active) {
            event->active = 0;
        }
    }
}
