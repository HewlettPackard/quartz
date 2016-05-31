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


void* worker(void* arg) 
{
    int latency_ns;

    latency_ns = measure_latency2(g_seed, g_nchains, g_nelems, g_element_size, g_access_size, g_from_node_id, g_to_node_id);
    printf("latency_ns: %d\n", latency_ns);

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
