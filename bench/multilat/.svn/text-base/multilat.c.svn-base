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
//#include <pthread.h>

#include "thread.h"
#include <sys/time.h>
#include "pmalloc.h"
#include "debug.h"
//#include "stat.h"


#define NDEBUG

//#ifndef NDEBUG
#include <sys/syscall.h>
//#endif

// packs the arguments received from user
typedef struct {
	int mem_refs_dram;
	int mem_refs_nvm;
	int interleave_dram;
	int interleave_nvm;
	//int from_node;
	//int to_node;
} arg_s;


// for multi thread management
#define MAX_NUM_THREADS 50
pthread_t thread_desc[MAX_NUM_THREADS];
//pthread_mutex_t mutex;


// for CPU cache trashing and pointer chasing
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


/*extern inline hrtime_t hrtime_cycles(void);
static inline void delay_cycles(hrtime_t cycles)
{
    hrtime_t start, stop;

    start = hrtime_cycles();
    do {
        stop = hrtime_cycles();
    } while (stop - start < cycles);
}*/


// for fixing thread affinity to a single CPU after allocating memory chains and binding it to the local or remote nodes
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
                          int mem_refs,               // number of pointers/elements to chase
                          uint64_t max_nelems,        // max number of available elements/pointers
                          int it_n,                   // seed to calculate the first pointer to chase, used to avoid repeating
                                                      // pointers during consecutive calls
	                      unsigned long *time_diff_ns) {
    uint64_t j, i;
    int nchains = SEED_IN;
    uint64_t sumv[MAX_NUM_CHAINS];
    uint64_t nextp[MAX_NUM_CHAINS];
    char *buf;
    uint64_t buf_size = 16384;
    int count = 0;
    uint64_t start;
    uint64_t it_limit;
    struct timespec time_start, time_end;

    assert(nchains < MAX_NUM_CHAINS);

    if (mem_refs <= 0) return 0;

    buf = (char*) malloc(buf_size);
    assert(buf != NULL);

    if (max_nelems > mem_refs) {
        it_limit = max_nelems / mem_refs;
    } else {
    	it_limit = 1;
    }
    it_n = it_n % it_limit;
    start = it_n * mem_refs;
    if ((start + mem_refs) > max_nelems) {
    	start = 0;
    }

    /* chase the pointers */
    if (nchains == 1) {
    	clock_gettime(CLOCK_MONOTONIC, &time_start);
        sumv[0] = 0;
        // chase pointers until the 'mem_refs' count, the pointer chasing will restart from beginning if 'mem_refs'
        // is greater than 'nelems'
        for (count = 0, i = start; count < mem_refs; i = element(C[0], i)->val, ++count) {
            __asm__("");
            sumv[0] += element(C[0], i)->val;
            if (access_size > element_size) {
                read_element(C[0], i, buf, buf_size);
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &time_end);
    }
//    else {
//        for (j=0; j < nchains; j++) {
//            sumv[j] = 0;
//            nextp[j] = 0;
//        }
//        for (; 0 != element(C[0], nextp[0])->val; ) {
//            for (j=0; j < nchains; j++) {
//                sumv[j] += element(C[j], nextp[j])->val;
//                if (access_size > element_size) {
//                    read_element(C[j], nextp[j], buf, buf_size);
//                }
//                nextp[j] = element(C[j], nextp[j])->val;
//            }
//        }
//    }

    *time_diff_ns = ((time_end.tv_sec * 1000000000) + time_end.tv_nsec) -
                    ((time_start.tv_sec * 1000000000) + time_start.tv_nsec);

    free(buf);
    return sumv[0];
}

void thread_iter(int dram_refs, int nvm_refs, int interleave_dram, int interleave_nvm) {
	long it_n;
	unsigned long time_dram, time_nvm, total_time_dram_ns, total_time_nvm_ns;
	uint64_t seed;
	uint64_t j;
	chain_t *C_dram[MAX_NUM_CHAINS];
	chain_t *C_nvm[MAX_NUM_CHAINS];
	int missing_dram_refs, missing_nvm_refs;
	int dram_stalls, nvm_stalls;
	struct timespec task_time_start, task_time_end;
	unsigned long task_time_diff_ns;
#ifndef NDEBUG
	pid_t tid = (pid_t) syscall(SYS_gettid);
#endif

	assert(NELEMS < UINT64_MAX);

    for (j=0; j < NCHAINS; j++) {
        seed = SEED_IN + j*j;
        C_dram[j] = alloc_chain(seed, NELEMS, 64LLU, 0, 0);
        C_nvm[j] = alloc_chain(seed, NELEMS, 64LLU, 0, 1);
        __asm__("");
    }

    bind_cpu(thread_self());

    // cache must be trashed after bind_cpu() call
    trash_cache(NELEMS);

    total_time_dram_ns = 0;
    total_time_nvm_ns = 0;

    missing_dram_refs = dram_refs;
    missing_nvm_refs = nvm_refs;

#ifndef NDEBUG
    printf("DRAM accesses to be made: %ld\n", dram_refs);
    printf("NVM accesses to be made: %ld\n", nvm_refs);
#endif

    //delay_cycles(8000000000);
    //printf("STARTING MEASURES\n");

    clock_gettime(CLOCK_MONOTONIC, &task_time_start);

    for (it_n = 0; (missing_dram_refs > 0) || (missing_nvm_refs > 0); ++it_n) {
    	__asm__("");

    	// calculate the number o memory accesses to be made on each memory type
    	if (missing_dram_refs > interleave_dram) {
    		missing_dram_refs -= interleave_dram;
    		dram_stalls = interleave_dram;
    	} else {
    		dram_stalls = missing_dram_refs;
    		missing_dram_refs = 0;
    	}

    	if (missing_nvm_refs > interleave_nvm) {
			missing_nvm_refs -= interleave_nvm;
			nvm_stalls = interleave_nvm;
		} else {
			nvm_stalls = missing_nvm_refs;
			missing_nvm_refs = 0;
		}

    	time_dram = 0;
    	time_nvm = 0;

    	// do memory accesses interleaved by dividing the number of accesses in smaller amount
    	// as configured by user
        force_ldm_stalls((chain_t **)&C_dram, 64LLU, 8, dram_stalls, NELEMS, it_n, &time_dram);
        force_ldm_stalls((chain_t **)&C_nvm, 64LLU, 8, nvm_stalls, NELEMS, it_n, &time_nvm);

        total_time_dram_ns += time_dram;
        total_time_nvm_ns += time_nvm;
#ifndef NDEBUG
        printf("%ld DRAM accesses took: %ld ns\n", dram_stalls, time_dram);
        printf("%ld NVM accesses took: %ld ns\n", nvm_stalls, time_nvm);
#endif
    }

    clock_gettime(CLOCK_MONOTONIC, &task_time_end);
    task_time_diff_ns = ((task_time_end.tv_sec * 1000000000) + task_time_end.tv_nsec) -
                        ((task_time_start.tv_sec * 1000000000) + task_time_start.tv_nsec);

    // the memory latency is the total time divided by the number of accesses for each memory type
    if (dram_refs > 0)
        total_time_dram_ns /= dram_refs;
    else
        total_time_dram_ns = 0;
    if (nvm_refs > 0)
        total_time_nvm_ns /= nvm_refs;
    else
        total_time_nvm_ns = 0;

    printf("DRAM latency: %ld ns\n", total_time_dram_ns);
    printf("NVM latency: %ld ns\n", total_time_nvm_ns);
    printf("Measure time: %.3lf ms\n", (double)task_time_diff_ns/1000000.0);
    
    printf("Expected time: %.3ld ms\n", ((total_time_dram_ns * dram_refs) + (total_time_nvm_ns * nvm_refs)) / 1000000);

    for (j=0; j < NCHAINS; j++) {
        free(C_dram[j]);
        free(C_nvm[j]);
    }
}

void *thread_fn(void *arg) {
	int interleave_dram = ((arg_s *) arg)->interleave_dram;
	int interleave_nvm = ((arg_s *) arg)->interleave_nvm;
	int dram_refs = ((arg_s *) arg)->mem_refs_dram;
	int nvm_refs = ((arg_s *) arg)->mem_refs_nvm;

	thread_iter(dram_refs, nvm_refs, interleave_dram, interleave_nvm);

	return 0;
}

void run_threads(int n_threads, int dram_refs, int nvm_refs, int interleaved_dram, int interleaved_nvm)
{
	pthread_attr_t attr;
    int i;
    arg_s args;

    if ((n_threads > MAX_NUM_THREADS) || (n_threads <= 0)) {
    	printf("INVALID RANGE:\n");
    	printf("\tMax number of threads is %d\n", MAX_NUM_THREADS);
    	exit(-1);
    }

    if (dram_refs < 0 || nvm_refs < 0 || interleaved_dram < 0 || interleaved_nvm < 0) {
    	printf("INVALID RANGE:\n");
    	printf("\tdram refs: %d, nvm refs: %d, interleaved dram refs: %d, interleaved nvm refs: %d\n",
    			dram_refs, nvm_refs, interleaved_dram, interleaved_nvm);
    	exit(-1);
    }

    if ((dram_refs > 0 && interleaved_dram == 0) || (nvm_refs > 0 && interleaved_nvm == 0)) {
    	printf("INVALID ARGUMENTS:\n");
    	printf("\tnumber of accesses in sequence cannot be zero if the number of accesses for the same memory type is greater than zero.\n");
    	exit(-1);
    }

    if (dram_refs < interleaved_dram) {
    	printf("INVALID ARGUMENTS:\n");
    	printf("\tnumber of DRAM accesses cannot be lower than the number of DRAM accesses in sequence\n");
    	exit(-1);
    }
    if (nvm_refs < interleaved_nvm) {
    	printf("INVALID ARGUMENTS:\n");
    	printf("\tnumber of NVM accesses cannot be lower than the number of NVM accesses in sequence\n");
    	exit(-1);
    }

    if (pthread_attr_init(&attr) != 0) {
		printf("pthread_attr_init failed");
		exit(-1);
	}

    //srand(time(NULL));

    args.interleave_dram = interleaved_dram;
    args.interleave_nvm = interleaved_nvm;
    args.mem_refs_dram = dram_refs;
    args.mem_refs_nvm = nvm_refs;

    for (i = 0; i < n_threads; ++i) {
	    pthread_create(&thread_desc[i], &attr, thread_fn, (void *)&args);
	}

    pthread_attr_destroy(&attr);

    for (i = 0; i < n_threads; ++i) {
        pthread_join(thread_desc[i], NULL);
    }
}

int main(int argn, char **argv)
{
    int dram_refs;
    int nvm_refs;
    int interleaved_dram;
    int interleaved_nvm;
    int n_threads;

    if (argn != 6) {
        printf("INVALID ARGUMENTS:\n");
        printf("\t%s [# threads] [# total dram accesses] [# total nvm accesses] [# dram accesses in sequence] [# nvm accesses in sequence]\n", argv[0]);
        return -1;
    }

    n_threads = atoi(argv[1]);
    dram_refs = atoi(argv[2]);
    nvm_refs = atoi(argv[3]);
    interleaved_dram = atoi(argv[4]);
    interleaved_nvm = atoi(argv[5]);

    run_threads(n_threads, dram_refs, nvm_refs, interleaved_dram, interleaved_nvm);

    return 0;
}
