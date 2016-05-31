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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include "model.h"
#include "thread.h"

#define MAX_NUM_THREADS 512

uint64_t g_seed, g_nchains, g_nelems, g_from_node_id, g_to_node_id, g_element_size, g_access_size;

extern int measure_latency2(uint64_t seedin, int nchains, size_t nelems, int element_size, int access_size, int from_node_id, int to_node_id);

static uint64_t safe_strtoull(const char *s) {
    char *ep;
    uint64_t r;
    assert(NULL != s && '\0' != *s);
    r = strtoull(s, &ep, 10);
    assert('\0' == *ep);
    return r;
}

extern latency_model_t latency_model;

#ifdef MEMLAT_SUPPORT
extern __thread int tls_hw_local_latency;
extern __thread int tls_hw_remote_latency;
extern __thread uint64_t tls_global_remote_dram;
extern __thread uint64_t tls_global_local_dram;

static inline uint64_t ns_to_cycles(int cpu_speed_mhz, int ns)
{
    return (cpu_speed_mhz * ns) / 1000;
}
#endif

void* worker(void* arg) 
{
    int latency_ns;
#ifdef MEMLAT_SUPPORT
    uint64_t exp_stalls;
    uint64_t calc_nvm_accesses;
    uint64_t detected_hw_lat;
    uint64_t actual_lat = 0;
    uint64_t total_time;
    uint64_t fixed_latency_ns = 0;
    uint64_t nvm_accesses = 0;
    uint64_t nvm_hw_latency;
#endif

    latency_ns = measure_latency2(g_seed, g_nchains, g_nelems, g_element_size, g_access_size, g_from_node_id, g_to_node_id);
    printf("latency_ns: %d ns\n", latency_ns);

#ifdef MEMLAT_SUPPORT
    total_time = g_nelems * latency_ns;
    if (thread_self()->virtual_node->dram_node != thread_self()->virtual_node->nvram_node) {
        detected_hw_lat = ns_to_cycles(thread_self()->cpu_speed_mhz, tls_hw_remote_latency);
        if (tls_global_remote_dram > 0) {
    	    actual_lat = thread_self()->stall_cycles / tls_global_remote_dram;
    	    fixed_latency_ns = total_time / tls_global_remote_dram;
    	    nvm_accesses = tls_global_remote_dram;
    	}
    	nvm_hw_latency = tls_hw_remote_latency;
    } else {
        detected_hw_lat = ns_to_cycles(thread_self()->cpu_speed_mhz, tls_hw_local_latency);
        if (tls_global_local_dram > 0) {
    	    actual_lat = thread_self()->stall_cycles / tls_global_local_dram;
    	    fixed_latency_ns = total_time / tls_global_local_dram;
    	    nvm_accesses = tls_global_local_dram;
    	}
    	nvm_hw_latency = tls_hw_local_latency;
    }
    exp_stalls = g_nelems * detected_hw_lat;
    calc_nvm_accesses = thread_self()->stall_cycles / detected_hw_lat;

    printf("target latency: %d ns\n", latency_model.read_latency);
    printf("Error: %3.1f%%\n", (double)(abs(latency_model.read_latency - latency_ns)*100) / (double)latency_model.read_latency);
    printf("target NVM accesses: %ld\n", g_nelems);
    printf("detected HW latency: %ld ns\n", nvm_hw_latency);
    printf("detected HW latency: %ld cycles (detected_hw_lat making use of cpu_speed_mhz)\n", detected_hw_lat);
    printf("expected CPU stalls: %ld cycles (target_nvm_accesses * detected_hw_lat)\n", exp_stalls);
    printf("actual CPU stalls: %ld cycles\n", thread_self()->stall_cycles);
    printf("calculated NVM accesses: %ld (actual_cpu_stalls / detected_hw_lat)\n", calc_nvm_accesses);
    if (nvm_accesses != 0) {
        printf("actual NVM accesses: %ld\n", nvm_accesses);
        printf("actual latency: %ld cyles (actual_stalls / actual_nvm_accesses)\n", actual_lat);
        printf("fixed measured latency: %ld ns (total_chasing_time / actual_nvm_accesses)\n", fixed_latency_ns);
        printf("fixed latency error: %3.1f%%\n", (double)(abs(latency_model.read_latency - fixed_latency_ns)*100) / (double)latency_model.read_latency);
    } else {
        fixed_latency_ns = total_time / calc_nvm_accesses;
        printf("fixed measured latency: %ld ns (total_chasing_time / calculated_nvm_accesses)\n", fixed_latency_ns);
        printf("fixed latency error: %3.1f%%\n", (double)(abs(latency_model.read_latency - fixed_latency_ns)*100) / (double)latency_model.read_latency);
    }
#endif
    return NULL;
}
int main(int argc, char *argv[]) {
	int i;
    uint64_t nthreads;
    pthread_t thread[MAX_NUM_THREADS];

    if (9 != argc) {
        fprintf(stderr, "usage: %s PRNGseed Nthreads Nchains Nelems SZelem SZaccess from_node to_node\n", argv[0]);
        return 1;
    }
    g_seed  = safe_strtoull(argv[1]);
    nthreads = safe_strtoull(argv[2]);
    g_nchains = safe_strtoull(argv[3]);
    g_nelems = safe_strtoull(argv[4]);
    g_element_size = safe_strtoull(argv[5]);
    g_access_size = safe_strtoull(argv[6]);
    g_from_node_id = safe_strtoull(argv[7]);
    g_to_node_id = safe_strtoull(argv[8]);

	for (i = 0; i< nthreads; i++) {
		pthread_create(&thread[i], NULL, worker, NULL);
    }
	for(i = 0 ; i < nthreads; i++) {
		pthread_join(thread[i], NULL);
    }
    return 0;
}
