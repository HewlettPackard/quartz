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
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "thread.h"
#include <sys/time.h>
#include "pmalloc.h"
#include "debug.h"
//#include "stat.h"


#ifndef NDEBUG
#include <sys/syscall.h>
#endif

typedef struct {
	int cs_n;
	int cs_duration;
	int out_cs_duration;
	int from_node;
	int to_node;
} arg_s;

#define MAX_NUM_THREADS 50
pthread_t thread_desc[MAX_NUM_THREADS];



#include <inttypes.h>
typedef struct {
	uint64_t val;
	char padding[0];
} element_t;

typedef struct {
    uint64_t   N;
    uint64_t   element_size;
    element_t* head;
} chain_t;
uint64_t trash_cache(uint64_t N);
chain_t* alloc_chain(uint64_t seedin, uint64_t N, uint64_t element_size, uint64_t node_i, uint64_t node_j);
element_t* element(chain_t* chain, uint64_t index);
void inline read_element(chain_t* chain, uint64_t index, char* buf, uint64_t buf_size);


// factor is 10 (could be more), to make sure we have a buffer much bigger than CPU cache
// the memory buffer is NOT shared among threads
// for now the cache size is hardcoded as 20 MB
#define NELEMS (10 * 20480000 / 64LLU)
#define PAGESZ 4096
#define MAX_NUM_CHAINS 16
//#undef USE_HUGETLB
#define SEED_IN 1
#define NCHAINS 1

pthread_mutex_t mutex;

static int max_number_of_cpus(void)
{
    int n, cpus = 2048;
    size_t setsize =  CPU_ALLOC_SIZE(cpus);
    cpu_set_t *set = CPU_ALLOC(cpus);
    if (!set)
        goto err;

	for (;;) {
		CPU_ZERO_S(setsize, set);
		/* the library version does not return size of cpumask_t */
		n = syscall(SYS_sched_getaffinity, 0, setsize, set);
		if (n < 0 && cpus < 1024 * 1024) {
		        CPU_FREE(set);
			cpus *= 2;
			set = CPU_ALLOC(cpus);
			if (!set)
				goto err;
			continue;
		}

	CPU_FREE(set);
	return n * 8;
	}
err:
	printf("cannot determine NR_CPUS");
	return 0;
}

static int bind_cpu(thread_t *thread) {
    size_t setsize;
    cpu_set_t *cur_cpuset;
    cpu_set_t *new_cpuset;

    int ncpus = max_number_of_cpus();

    if (thread == NULL) {
        // if thread is NULL it means the emulator is disabled, return without setting CPU affinity
        //printf("thread self is null");
        return 0;
    }

    if (ncpus == 0) {
    	return 1;
    }

    setsize = CPU_ALLOC_SIZE(ncpus);
    cur_cpuset = CPU_ALLOC(ncpus);
    new_cpuset = CPU_ALLOC(ncpus);
    CPU_ZERO_S(setsize, cur_cpuset);
    CPU_ZERO_S(setsize, new_cpuset);
    CPU_SET_S(thread->cpu_id, setsize, new_cpuset);

    if (pthread_getaffinity_np(thread->pthread, setsize, cur_cpuset) != 0) {
        DBG_LOG(ERROR, "Cannot get thread tid [%d] affinity, pthread: 0x%lx on processor %d\n",
        		thread->tid, thread->pthread, thread->cpu_id);
        return 1;
    }

    if (CPU_EQUAL(cur_cpuset, new_cpuset)) {
        //printf("No need to bind CPU\n");
    	return 0;
    }

    DBG_LOG(INFO, "Binding thread tid [%d] pthread: 0x%lx on processor %d\n", thread->tid, thread->pthread, thread->cpu_id);

    if (pthread_setaffinity_np(thread->pthread, setsize, new_cpuset) != 0) {
        DBG_LOG(ERROR, "Cannot bind thread tid [%d] pthread: 0x%lx on processor %d\n", thread->tid, thread->pthread, thread->cpu_id);
        return 1;
    }

    return 0;
}

uint64_t force_ldm_stalls(chain_t **C,
                          int element_size,
                          int access_size,
                          int duration,             // number of pointers/elements to chase
                          uint64_t nelems,          // max number of available elements/pointers
                          int it_n) {               // seed to calculate the first pointer to chase, used to avoid repeating
                                                    // pointers during consecutive calls
    uint64_t j, i;
    int nchains = SEED_IN;
    uint64_t sumv[MAX_NUM_CHAINS];
    uint64_t nextp[MAX_NUM_CHAINS];
    char *buf;
    uint64_t buf_size = 16384;
    int count = 0;
    uint64_t start;
    uint64_t it_limit;

    assert(nchains < MAX_NUM_CHAINS);

    if (duration <= 0) return 0;

    // TODO: ignore the use of buf?
    // TODO: ignore more than one chain?
    buf = (char*) malloc(buf_size);
    assert(buf != NULL);

    if (nelems > duration) {
        it_limit = nelems / duration;
    } else {
    	it_limit = 1;
    }
    it_n = it_n % it_limit;
    start = it_n * duration;
    if ((start + duration) > nelems) {
    	start = 0;
    }

    /* chase the pointers */
    if (nchains == 1) {
        sumv[0] = 0;
        // chase pointers until the 'duration' count, the pointer chasing will restart from beginning if duration
        // is greater than 'nelems'
        for (count = 0, i = start; count < duration; i = element(C[0], i)->val, ++count) {
            __asm__("");
            sumv[0] += element(C[0], i)->val;
            if (access_size > element_size) {
                read_element(C[0], i, buf, buf_size);
            }
        }
    } else {
        for (j=0; j < nchains; j++) {
            sumv[j] = 0;
            nextp[j] = 0;
        }
        for (; 0 != element(C[0], nextp[0])->val; ) {
            for (j=0; j < nchains; j++) {
                sumv[j] += element(C[j], nextp[j])->val;
                if (access_size > element_size) {
                    read_element(C[j], nextp[j], buf, buf_size);
                }
                nextp[j] = element(C[j], nextp[j])->val;
            }
        }
    }

    free(buf);
    return sumv[0];
}

void iter(int cs_n, int cs_duration, int out_cs_duration, int from_node, int to_node) {
	long it_n;
	struct timespec time_start, time_end;
	unsigned long diff_us;
	uint64_t seed;
	uint64_t j;
	chain_t *C[MAX_NUM_CHAINS];
#ifndef NDEBUG
	pid_t tid = (pid_t) syscall(SYS_gettid);
#endif

	DBG_LOG(INFO, "\t: from node: %d to node: %d\n", from_node, to_node);

	assert(NELEMS < UINT64_MAX);

    for (j=0; j < NCHAINS; j++) {
        seed = SEED_IN + j*j;
        C[j] = alloc_chain(seed, NELEMS, 64LLU, from_node, to_node);
        __asm__("");
    }

    bind_cpu(thread_self());

    trash_cache(NELEMS);

    for (it_n = 0; it_n < cs_n; ++it_n) {
    	__asm__("");
        pthread_mutex_lock(&mutex);
#ifndef NDEBUG
        clock_gettime(CLOCK_MONOTONIC, &time_start);
#endif
        // critical section
        // make cs_duration random memory accesses and leave
        force_ldm_stalls((chain_t **)&C, 64LLU, 8, cs_duration, NELEMS, it_n);
#ifndef NDEBUG
        clock_gettime(CLOCK_MONOTONIC, &time_end);
#endif
        pthread_mutex_unlock(&mutex);

        // outside critical section
        force_ldm_stalls((chain_t **)&C, 64LLU, 8, out_cs_duration, NELEMS, (it_n+1)*2);

#ifndef NDEBUG
        diff_us = ((time_end.tv_sec * 1000000) + (time_end.tv_nsec / 1000)) -
                  ((time_start.tv_sec * 1000000) + (time_start.tv_nsec / 1000));
        DBG_LOG(INFO, "\tthread [%d] critical section took %lu usec\n", tid, diff_us);
#endif
//        if ((it_n + 1) % out_cs_duration == 0) {
////            usleep(1);
////            pthread_yield();
//            sched_yield();
//        }
    }

    for (j=0; j < NCHAINS; j++) {
        free(C[j]);
    }
}

void *thread_fn(void *arg) {
	int cs_n = ((arg_s *) arg)->cs_n;
	int cs_duration = ((arg_s *) arg)->cs_duration;
	int out_cs_duration = ((arg_s *) arg)->out_cs_duration;
	int from_node = ((arg_s *) arg)->from_node;
	int to_node = ((arg_s *) arg)->to_node;

	iter(cs_n, cs_duration, out_cs_duration, from_node, to_node);

	return 0;
}

void manage_threads(int n_threads, int cs_n, int cs_duration, int out_cs_duration, int from_node, int to_node)
{
	pthread_attr_t attr;
    int i;
    arg_s args;

    if ((n_threads > MAX_NUM_THREADS) || (n_threads <= 0)) {
    	printf("INVALID RANGE:\n");
    	printf("\tMax number of threads is %d\n", MAX_NUM_THREADS);
    	exit(-1);
    }

    if (cs_n <= 0 || cs_duration <= 0 || out_cs_duration < 0) {
    	printf("INVALID RANGE:\n");
    	printf("\tcritical sections: %d, cs level: %d, out cs level: %d\n", cs_n, cs_duration, out_cs_duration);
    	exit(-1);
    }

    pthread_mutex_init(&mutex, NULL);

    if (pthread_attr_init(&attr) != 0) {
		printf("pthread_attr_init failed");
		exit(-1);
	}

    srand(time(NULL));

    args.cs_duration = cs_duration;
    args.cs_n = cs_n;
    args.out_cs_duration = out_cs_duration;
    args.from_node = from_node;
    args.to_node = to_node;

    for (i = 0; i < n_threads; ++i) {
	    pthread_create(&thread_desc[i], &attr, thread_fn, (void *)&args);
	}

    pthread_attr_destroy(&attr);

    for (i = 0; i < n_threads; ++i) {
        pthread_join(thread_desc[i], NULL);
    }

    pthread_mutex_destroy(&mutex);
}

int main(int argn, char **argv)
{
    int n_threads;
    int cs_n;
    int cs_duration;
    //int cs_n_before_yield;
    int out_cs_duration;
    int from_node;
    int to_node;

    if (argn != 7) {
        printf("INVALID ARGUMENTS:\n");
        printf("\t%s [# threads] [# critical sections per thread] [size of each critical section] "
        	   "[size of computation outside critical section] [from_node] [to_node]\n", argv[0]);
        return -1;
    }

    n_threads = atoi(argv[1]);
    cs_n = atoi(argv[2]);
    cs_duration = atoi(argv[3]);
    //cs_n_before_yield = atoi(argv[4]);
    out_cs_duration = atoi(argv[4]);
    from_node = atoi(argv[5]);
    to_node = atoi(argv[6]);

    manage_threads(n_threads, cs_n, cs_duration, out_cs_duration, from_node, to_node);

//    stats_report();

    return 0;
}
