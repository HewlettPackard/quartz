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
#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>

#include "utlist.h"
#include "stat.h"
#include "thread.h"
#include "interpose.h"
#include "model.h"

thread_manager_t* get_thread_manager();
hrtime_t cycles_to_us(int cpu_speed_mhz, hrtime_t cycles);

#ifdef USE_STATISTICS
void stats_set_init_time(double init_time_us) {
	thread_manager_t* thread_manager = get_thread_manager();

	__lib_pthread_mutex_lock(&thread_manager->mutex);
	thread_manager->stats.init_time_us = init_time_us;
	__lib_pthread_mutex_unlock(&thread_manager->mutex);
}

void stats_enable(config_t *cfg) {
	thread_manager_t* thread_manager = get_thread_manager();

    __cconfig_lookup_bool(cfg, "statistics.enable", &thread_manager->stats.enabled);
    if (__cconfig_lookup_string(cfg, "statistics.file", &thread_manager->stats.output_file) == CONFIG_FALSE) {
    	__lib_pthread_mutex_lock(&thread_manager->mutex);
    	thread_manager->stats.output_file = NULL;
    	__lib_pthread_mutex_unlock(&thread_manager->mutex);
    }
}

static char *get_current_time() {
    time_t curtime;
    char *str_time;

    time(&curtime);
    str_time = ctime(&curtime);
    str_time[strlen(str_time) - 1] = 0;

    return str_time;
}

static inline hrtime_t ns_to_cycles(int cpu_speed_mhz, int ns)
{
    return (cpu_speed_mhz * ns) / 1000;
}

extern __thread int tls_hw_local_latency;
extern __thread int tls_hw_remote_latency;

static void show_thread_stats(thread_t *thread, FILE *out_file) {
    uint64_t fixed_value;
    uint64_t cycles;

    fprintf(out_file, "\tThread id [%d]\n", thread->tid);
    fprintf(out_file, "\t\t: cpu id: %d\n", thread->cpu_id);
    fprintf(out_file, "\t\t: spawn timestamp: %lu\n", thread->stats.register_timestamp);
    fprintf(out_file, "\t\t: termination timestamp: %lu\n", thread->stats.unregister_timestamp);
    fixed_value = thread->stats.unregister_timestamp > 0 ? (thread->stats.unregister_timestamp - thread->stats.register_timestamp) : 0;
    fprintf(out_file, "\t\t: execution time: %lu usecs\n", fixed_value);
    fprintf(out_file, "\t\t: stall cycles: %lu\n", thread->stats.stall_cycles);

    if (thread->virtual_node->dram_node != thread->virtual_node->nvram_node &&
                latency_model.pmc_remote_dram) {
        cycles = ns_to_cycles(thread->cpu_speed_mhz, tls_hw_remote_latency);
        fixed_value = cycles ? thread->stats.stall_cycles / cycles : 0;
    }
    else {
        cycles = ns_to_cycles(thread->cpu_speed_mhz, tls_hw_local_latency);
        fixed_value = cycles ? thread->stats.stall_cycles / cycles : 0;
    }
    fprintf(out_file, "\t\t: NVM accesses: %lu\n", fixed_value);


    fprintf(out_file, "\t\t: latency calculation overhead cycles: %lu\n", thread->stats.overhead_cycles);
    fprintf(out_file, "\t\t: injected delay cycles: %lu\n", thread->stats.delay_cycles);
    if (thread->cpu_speed_mhz) {
        fprintf(out_file, "\t\t: injected delay in usec: %lu\n", cycles_to_us(thread->cpu_speed_mhz, thread->stats.delay_cycles));
    }
    fprintf(out_file, "\t\t: longest epoch duration: %lu usec\n", thread->stats.longest_epoch_duration_us);
    fixed_value = (thread->stats.shortest_epoch_duration_us == UINT64_MAX) ? 0 : thread->stats.shortest_epoch_duration_us;
    fprintf(out_file, "\t\t: shortest epoch duration: %lu usec\n", fixed_value);
    fixed_value = thread->stats.epochs ? (thread->stats.overall_epoch_duration_us / thread->stats.epochs) :
    		thread->stats.overall_epoch_duration_us;
    fprintf(out_file, "\t\t: average epoch duration: %lu usec\n", fixed_value);
    fprintf(out_file, "\t\t: number of epochs: %lu\n", thread->stats.epochs);
    fprintf(out_file, "\t\t: epochs which didn't reach min duration: %lu\n", thread->stats.min_epoch_not_reached);
    fprintf(out_file, "\t\t: static epochs requested: %lu\n", thread->stats.signals_sent);
}

void stats_report() {
    thread_t *thread;
    FILE *out_file;
    uint64_t running_threads = 0;
    thread_manager_t* thread_manager = get_thread_manager();
    uint64_t terminated_threads;

    if (!thread_manager) return;
    if (!thread_manager->stats.enabled) return;

    if (thread_manager->stats.output_file) {
        out_file = fopen(thread_manager->stats.output_file, "a");
        if (!out_file) {
            fprintf(stderr, "Failed to open statistics file for writing: %s\n", thread_manager->stats.output_file);
            return;
        }
    } else {
        out_file = stdout;
    }

    __lib_pthread_mutex_lock(&thread_manager->mutex);
    LL_FOREACH(thread_manager->thread_list, thread) {
        running_threads++;
    }
    __lib_pthread_mutex_unlock(&thread_manager->mutex);

    fprintf(out_file, "\n\n===== STATISTICS (%s) =====\n\n", get_current_time());
    if (!latency_model.inject_delay) {
    	fprintf(out_file, "WARNING: delay injection is disabled\n");
    }
    fprintf(out_file, "PID: %d\n", getpid());
    fprintf(out_file, "Initialization duration: %lu usec\n", thread_manager->stats.init_time_us);
    fprintf(out_file, "Running threads: %lu\n", running_threads);
    terminated_threads = thread_manager->stats.n_threads > 0 ? (thread_manager->stats.n_threads - running_threads) : 0;
    fprintf(out_file, "Terminated threads: %lu\n", terminated_threads);
    fprintf(out_file, "\n");

    fprintf(out_file, "== Running threads == \n");

    __lib_pthread_mutex_lock(&thread_manager->mutex);
    LL_FOREACH(thread_manager->thread_list, thread) {
    	show_thread_stats(thread, out_file);
    }
    __lib_pthread_mutex_unlock(&thread_manager->mutex);

    fprintf(out_file, "\n== Terminated threads == \n");

    __lib_pthread_mutex_lock(&thread_manager->mutex);
    LL_FOREACH(thread_manager->stats.thread_list, thread) {
    	show_thread_stats(thread, out_file);
    }
    __lib_pthread_mutex_unlock(&thread_manager->mutex);

    if (out_file != stdout) {
        fclose(out_file);
    }
}
#endif

double sum(double array[], int n)
{
    int i;
    double s = 0;

    for (i=0; i<n; i++) {
        s += array[i];
    }
    return s;
}

// returns sum of x . y
double sumxy(double x[], double y[], int n)
{
    int i;
    double s = 0;

    for (i=0; i<n; i++) {
        s += x[i] * y[i];
    }
    return s;
}


double avg(double array[], int n)
{
    double s;

    s = sum(array, n);
    return s/n;
}

double slope(double x[], double y[], int n)
{
    double sumxy_;
    double sumx2;
    double sumx;
    double sumy;
    double m; 

    sumxy_ = sumxy(x, y, n);
    sumx2 = sumxy(x, x, n);
    sumx = sum(x, n);
    sumy = sum(y, n);

    m = (n * sumxy_ - sumx * sumy) / 
        (n * sumx2 - sumx*sumx);
    return m;
}
