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
#ifndef __STATISTICS_H
#define __STATISTICS_H

//#include <sys/types.h>
#include <stdint.h>
#include "config.h"

#ifdef USE_STATISTICS
struct thread_s;

typedef struct {
    int enabled;
    struct thread_s* thread_list;
    uint64_t n_threads;
    uint64_t init_time_us;
    char *output_file;
} stats_t;

typedef struct {
    uint64_t stall_cycles;
    uint64_t overhead_cycles;
    uint64_t delay_cycles;
    uint64_t signals_sent;
    uint64_t epochs;
    double last_epoch_timestamp;
    uint64_t shortest_epoch_duration_us;
    uint64_t longest_epoch_duration_us;
    uint64_t overall_epoch_duration_us;
    uint64_t min_epoch_not_reached;
    uint64_t register_timestamp;
    uint64_t unregister_timestamp;
} thread_stats_t;

void stats_enable(config_t *cfg);
void stats_set_init_time(double init_time_us);
void stats_report();
#endif

double sum(double array[], int n);
double sumxy(double x[], double y[], int n);
double avg(double array[], int n);
double slope(double x[], double y[], int n);

#endif /* __STATISTICS_H */
