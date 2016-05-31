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
#ifndef __CPU_PMC_H
#define __CPU_PMC_H

#include "cpu/cpu.h"

#define DECLARE_ENABLE_PMC(prefix, name) int prefix##_create_pmc_##name(struct pmc_events_s* events, struct pmc_event_s* event)
#define DECLARE_CLEAR_PMC(prefix, name) void prefix##_clear_pmc_##name(struct pmc_event_s* event)
#define DECLARE_READ_PMC(prefix, name) uint64_t prefix##_read_pmc_##name(struct pmc_event_s* event)
#define ENABLE_PMC_FNAME(prefix, name) prefix##_create_pmc_##name
#define CLEAR_PMC_FNAME(prefix, name) prefix##_clear_pmc_##name
#define READ_PMC_FNAME(prefix, name) prefix##_read_pmc_##name

#define PMC_HW_EVENT(name, os_name, encoding)  { name, os_name, encoding, 0, 0},
#define PMC_EVENT(name, prefix)  { #name, NULL, 0, 0, ENABLE_PMC_FNAME(prefix, name), CLEAR_PMC_FNAME(prefix, name), READ_PMC_FNAME(prefix, name)},

#define PMC_EVENTS_PTR(prefix) &prefix##_pmc_events

#define PMC_EVENTS(prefix, num_hw_cntrs)          \
  pmc_hw_event_t prefix##_known_hw_event[] = {    \
    FOREACH_PMC_HW_EVENT(PMC_HW_EVENT)            \
    {NULL, NULL, 0, 0, 0}                         \
  };                                              \
  pmc_event_t prefix##_known_event[] = {          \
    FOREACH_PMC_EVENT(PMC_EVENT, prefix)          \
    {NULL, NULL, 0, 0, NULL, NULL, NULL}          \
  };                                              \
  pmc_events_t prefix##_pmc_events = {            \
    num_hw_cntrs,                                 \
    prefix##_known_hw_event,                      \
    prefix##_known_event                          \
  };

#define ASSIGN_PMC_HW_EVENT_TO_ME(name, local_id)                                   \
  if (assign_pmc_hw_event_to_event(events, name, event, local_id) != E_SUCCESS) {   \
    release_all_pmc_hw_events_of_event(event);                                      \
  }

#define READ_MY_HW_EVENT_DIFF(local_id) read_pmc_hw_event_diff(event->hw_events[local_id])
#define READ_MY_HW_EVENT_CUR(local_id) read_pmc_hw_event_cur(event->hw_events[local_id])

typedef struct {
    char* name;
    char* os_name; // perf name if known
    uint64_t encoding;
    int active;
    int hw_cntr_id;
    uint64_t* last_val; // array holding the last read values per processor (useful to calculate the diff since the last read)
} pmc_hw_event_t;

typedef struct pmc_event_s {
    const char* name;
    pmc_hw_event_t** hw_events;
    int num_hw_events;
    int active;
    int (*enable)(struct pmc_events_s* events, struct pmc_event_s* event);
    void (*clear)(struct pmc_event_s* event);
    uint64_t (*read)(struct pmc_event_s* event);
} pmc_event_t;

typedef struct pmc_events_s {
    int num_avail_hw_cntrs; 
    pmc_hw_event_t* known_hw_events;
    pmc_event_t* known_events;
} pmc_events_t;

pmc_hw_event_t* enable_pmc_hw_event(pmc_events_t* events, const char* name);
void disable_pmc_hw_event(pmc_events_t* events, const char* name);
void clear_pmc_hw_event(pmc_hw_event_t* event);
uint64_t read_pmc_hw_event_cur(pmc_hw_event_t* event);
uint64_t read_pmc_hw_event_diff(pmc_hw_event_t* event);
int assign_pmc_hw_event_to_event(pmc_events_t* events, const char* name, pmc_event_t* event, int local_id);
void release_all_pmc_hw_events_of_event(pmc_event_t* event);

pmc_event_t* enable_pmc_event(cpu_model_t* cpu, const char* name);
void disable_pmc_event(cpu_model_t* cpu, const char* name);

static inline void clear_pmc_event(pmc_event_t* event)
{
    event->clear(event);
}

//#include "debug.h"

static inline uint64_t read_pmc_event(pmc_event_t* event)
{
    uint64_t ret;
    ret = event->read(event);
    return ret;
}

#endif /* __CPU_PMC_H */
