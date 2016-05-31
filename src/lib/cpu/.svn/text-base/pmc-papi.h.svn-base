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

#include <stdint.h>


// Usually the architectures support up to 4 counters enabled at the same
// time per core when HT is enabled
#define MAX_NUM_EVENTS 4

typedef uint64_t (*read_stalls_t)(void);

typedef struct {
	const char **native_events;
	read_stalls_t read_stalls_events_local;
	read_stalls_t read_stalls_events_remote;
} pmc_event_t;

int pmc_init();
void pmc_shutdown();
int pmc_create_event_set_local_thread();
void pmc_destroy_event_set_local_thread();
int pmc_register_event_local_thread(const char *event_name);
int pmc_events_start_local_thread();
void pmc_events_stop_local_thread();
int pmc_events_read_local_thread(long long *values);

int pmc_register_thread();
int pmc_unregister_thread();

#endif /* __CPU_PMC_H */
