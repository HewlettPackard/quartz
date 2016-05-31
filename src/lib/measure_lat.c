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
/*
 * Originally developed by Terence Kelly with contributions from Haris Volos
 */

#include <string.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <numa.h>
#include <numaif.h>
#include <math.h>
#include "cpu/cpu.h"
#include "error.h"
#include "model.h"

#define P  (void)printf
#define FP (void)fprintf

#define PAGESZ 4096

#define MAX_NUM_CHAINS 16

#undef USE_HUGETLB

#ifdef MEMLAT_SUPPORT
extern __thread uint64_t tls_global_remote_dram;
extern __thread uint64_t tls_global_local_dram;
#endif

typedef struct {
	uint64_t val;
	char padding[0];
} element_t;

typedef struct {
    uint64_t   N;
    uint64_t   element_size;
    element_t* head;
} chain_t;

inline uint64_t min(uint64_t a, uint64_t b)
{
    return a < b ? a : b;
}

/* G. Marsaglia, 2003.  "Xorshift RNGs", Journal of Statistical
   Software v. 8 n. 14, pp. 1-6, discussed in _Numerical Recipes_
   3rd ed. */
static uint64_t prng(uint64_t* seed) {
    uint64_t x = *seed;
    x ^= x >> 21;
    x ^= x << 35;
    x ^= x >>  4;
    *seed = x;
    return x;
}

static uint64_t T(void) {
    struct timeval tv;

#ifndef NDEBUG
    int r =
#endif
        gettimeofday(&tv, NULL);

    assert(0 == r);

    return (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
}

element_t* element(chain_t* chain, uint64_t index) 
{
    char* p = (char*) chain->head + index * chain->element_size;
    return (element_t *) p;
}

void inline read_element(chain_t* chain, uint64_t index, char* buf, uint64_t buf_size)
{
    uint64_t i;
    element_t *elem = element(chain, index);
    buf_size = min(chain->element_size, buf_size);
    
    memcpy(buf, &elem->padding[0], buf_size - sizeof(elem->val));
    for (i = buf_size; i <= chain->element_size - buf_size; i += buf_size) {
        memcpy(buf, &elem->padding[i], buf_size);
    }
}

chain_t* alloc_chain(uint64_t seedin, uint64_t N, uint64_t element_size, uint64_t node_i, uint64_t node_j)
{
    uint64_t sum, p, i;
    element_t *B;
    char *A, *Aaligned, *M;
    uint64_t seed = seedin;
    chain_t* chain;
#ifndef NDEBUG
    long mbind_result;
#endif
    /* fill B[] with random permutation of 1..N */
    chain = (chain_t*) malloc(sizeof(chain_t));
    chain->N = N;
    chain->element_size = element_size;
    Aaligned = A = (char *) malloc(2 * PAGESZ + N * sizeof(element_t));
    assert(NULL != A);
    while ( 0 != (Aaligned - (char *)0) % PAGESZ )
        Aaligned++;
    B = (element_t *) Aaligned;
    for (i = 0; i < N; i++)
        B[i].val = 1+i;
    for (i = 0; i < N; i++) {
        uint64_t r, t;
        r = prng(&seed);
        r = r % N;  /* should be okay for N << 2^64 */
        t = B[i].val;
        B[i].val = B[r].val;
        B[r].val = t;
    }

    sum = 0;
    for (i = 0; i < N; i++)
      sum += B[i].val;
    assert((N+1)*N/2 == sum);  /* Euler's formula */

    /* set up C[] such that "chasing pointers" through it visits
       every element exactly once */
#ifdef USE_HUGETLB
    M = (char*) mmap(NULL, 2 * PAGESZ + (1+N) * element_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_HUGETLB, -1, 0);
#else
    M = (char*) mmap(NULL, 2 * PAGESZ + (1+N) * element_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
#endif
    assert(NULL != M);
    while ( 0 != (M - (char *)0) % PAGESZ )
      M++;
    numa_run_on_node(node_i);
    uint64_t nodemask = 1 << node_j;
#ifndef NDEBUG
    mbind_result =
#endif
        mbind(M, N*element_size, MPOL_BIND, &nodemask, 64, MPOL_MF_MOVE);

    assert(mbind_result == 0);

    bzero(M, N*element_size); // force physical memory allocation
    chain->head = (element_t *) M;
    for (i = 0; i < N; i++) {
        element(chain, i)->val = UINT64_MAX;
    }
    p = 0;
    for (i = 0; i < N; i++) {
        p = element(chain, p)->val = B[i].val;
    }
    element(chain, p)->val = 0;
    for (i = 0; i <= N; i++) {
        assert(N >= element(chain, i)->val);
    }
    free(A);
    return chain;
}


uint64_t trash_cache(uint64_t N)
{
    uint64_t T1, i, sum;
    char* A;
    char* ptr;
    element_t* B;
    ptr = A = (char *) malloc(2 * PAGESZ + N * sizeof(element_t));
    assert(NULL != A);
    while ( 0 != (A - (char *)0) % PAGESZ ) {
        A++;
        __asm__(""); /* prevent optimizer from removing loop */
    }
    B = (element_t *)A;

    /* trash the CPU cache */
    T1 = T() % 1000;
    for (i = 0; i < N; i++) {
        B[i].val = T1 * i + i % (T1+1);
        __asm__(""); /* prevent optimizer from removing loop */
    }
    sum = 0;
    for (i = 0; i < N; i++) {
        sum += B[i].val;
        __asm__(""); /* prevent optimizer from removing loop */
    }
    free(ptr);
    return sum;
}


int __measure_latency(uint64_t seedin, int nchains, size_t nelems, int element_size, int access_size, int from_node_id, int to_node_id) 
{
    uint64_t seed, j, i, T1, T2;
    uint64_t sumv[MAX_NUM_CHAINS];
    uint64_t nextp[MAX_NUM_CHAINS];
    chain_t *C[MAX_NUM_CHAINS];
    char *buf;
    uint64_t buf_size = 16384;

    assert(nelems < UINT64_MAX);
    assert(nchains < MAX_NUM_CHAINS);

    DBG_LOG(INFO, "measuring latency: nchains %d, nelems %zu, elem_sz %d, access_sz %d, from_node_id %d, to_node_id %d\n", nchains, nelems, element_size, access_size, from_node_id, to_node_id);

    for (j=0; j < nchains; j++) {
        seed = seedin + j*j;
        C[j] = alloc_chain(seed, nelems, element_size, from_node_id, to_node_id);
    }

    trash_cache(nelems);

    buf = (char*) malloc(buf_size);
    assert(buf != NULL);
#ifdef MEMLAT_SUPPORT
    tls_global_remote_dram = 0;
    tls_global_local_dram = 0;
#endif

    /* chase the pointers */
    if (nchains == 1) {
        T1 = T();
        sumv[0] = 0;
        for (i = 0; 0 != element(C[0], i)->val; i = element(C[0], i)->val) {
            sumv[0] += element(C[0], i)->val;
            if (access_size > element_size) {
                read_element(C[0], i, buf, buf_size);
            }
        }
        T2 = T();
    } else {
        T1 = T();
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
        T2 = T();
    }
    assert((nelems+1)*nelems/2 == sumv[0]);  /* Euler's formula */
    uint64_t time_per_op_ns = ((T2-T1)*1000)/nelems;

    DBG_LOG(INFO, "measuring latency: latency is %lu ns\n", time_per_op_ns);

    for (j=0; j < nchains; j++) {
        free(C[j]);
    }
    free(buf);

    return time_per_op_ns;
}

int measure_latency(cpu_model_t* cpu, int from_node_id, int to_node_id) 
{
    size_t factor = 10; // this needs to be large enough to ensure we always miss in the LLC cache
    size_t element_size = 64LLU;
    size_t access_size = 8;
    size_t nelems = factor * cpu->llc_size_bytes / element_size;
    
    return __measure_latency(1, 1, nelems, element_size, access_size, from_node_id, to_node_id);
}

int measure_latency2(uint64_t seedin, int nchains, size_t nelems, int element_size, int access_size, int from_node_id, int to_node_id) 
{
    if (nelems*element_size < cpu_llc_size_bytes()) { 
        DBG_LOG(WARNING, "warning:  #elements == %" PRIu64 " seems small!\n", nelems);
    }

    return __measure_latency(seedin, nchains, nelems, element_size, access_size, from_node_id, to_node_id);
}

#ifdef CALIBRATION_SUPPORT

#define TOLERATED_DEVIATION_PERCENTAGE 5  // maximum deviation acceptable for the target latency
#define CALIBRATION_STEP_SIZE 0.05        // max ns step size to calibrate the CPU stalls
#define CALIBRATION_FINEST_STEP 0.01      // min (finest) ns step size to calibrate the CPU stalls
#define MAX_TOLERATED_BAD_STEPS 2         // max number of bad steps in the calibration, before the calibration inverts the value to increment
#define NELEMS 10000000
#define SEED_IN 1
#define NCHAINS 1
#define ELEM_SIZE 64LLU
#define ACCESS_SIZE 8
#define FILE_CALIB_LOCAL "/tmp/local_latency_calibration"
#define FILE_CALIB_REMOTE "/tmp/remote_latency_calibration"

static int calibrate_load_from_file(virtual_node_t *virtual_node) {
    FILE *fp = NULL;
    char *file_name = NULL;
    char *line = NULL;
    size_t len;
    double correction_factor;
    int status = E_ERROR;

    if (virtual_node->dram_node == virtual_node->nvram_node) {
    	file_name = FILE_CALIB_LOCAL;
    } else {
    	file_name = FILE_CALIB_REMOTE;
    }

    if (access(file_name, R_OK | W_OK) == 0) {
        // calibration file is available, check if the current target latency is mapped
        if ((fp = fopen(file_name, "r"))) {
            if (getline(&line, &len, fp) != -1) {
                if (sscanf(line, "%lf", &correction_factor) == 1) {
                    // set CPU stalls factor to the read value
                    latency_model.stalls_calibration_factor = correction_factor;
                    DBG_LOG(INFO, "CALIBRATION: factor loaded from file (%s) (%f)\n",
                            file_name, correction_factor);
                    status = E_SUCCESS;
                }
            }

            if (line) free(line);
            fclose(fp);
        }
    }

    return status;
}

static void calibrate_save_to_file(virtual_node_t *virtual_node, double correction_factor) {
	char *file_name;
	FILE *fp;

	if (virtual_node->dram_node == virtual_node->nvram_node) {
		file_name = FILE_CALIB_LOCAL;
	} else {
		file_name = FILE_CALIB_REMOTE;
	}

	// calibration file is available, check if the current target latency is mapped
	if ((fp = fopen(file_name, "a"))) {
		// it is assumed this line is not yet present in the file
		fprintf(fp, "%f\n", correction_factor);
		DBG_LOG(INFO, "CALIBRATION: factor saved to file (%s) (%f)\n",
                file_name, correction_factor);
		fclose(fp);
	}
}

static int diff_target_latencies(int measured_latency, int target_latency) {
    int diff = target_latency - measured_latency;
    return abs(diff);
}

static double calibrate(virtual_node_t *virtual_node, double step_value, int from_node, int to_node) {
    int measured;
    int best_diff_latency;
    double best_factor = 0;
    int diff;
    int bad_step_count = 0;
    int close_value;
    int calib_done;

    // force a change in correction factor and measure latency
    // each step will increment the or decrement the factor
    // at the end we have a calibrated correction factor for the CPU stalls

    DBG_LOG(INFO, "CALIBRATION: for nodes (dram %d, nvram %d)\n", from_node, to_node);
    best_diff_latency = INT32_MAX;
    close_value = 0;
    calib_done = 0;

    while(!calib_done) {
        measured = measure_latency2(SEED_IN, NCHAINS, NELEMS, ELEM_SIZE, ACCESS_SIZE, from_node, to_node);
        DBG_LOG(INFO, "CALIBRATION: measured latency (%d)\n", measured);

        diff = diff_target_latencies(measured, latency_model.read_latency);
        if (diff < best_diff_latency) {
        	// best measured latency so far
            bad_step_count = 0;
            best_diff_latency = diff;
            best_factor = latency_model.stalls_calibration_factor;
            // check if the diff is less or equal than the configured percentage of the target latency
            if (diff <= (latency_model.read_latency * TOLERATED_DEVIATION_PERCENTAGE / 100)) {
                DBG_LOG(INFO, "CALIBRATION: got a close latency value (factor %lf)\n", best_factor);
                close_value = 1;
            }
        } else if (diff >= best_diff_latency) {
        	// measure latency is getting worse
            if (close_value && bad_step_count == 0) {
            	// if we have a close_value, return it
                calib_done = 1;
            } else {
            	// otherwise let's give retries
                ++bad_step_count;
                if (bad_step_count >= MAX_TOLERATED_BAD_STEPS) {
                    // this calibration method seem to be moving to the wrong direction
                    // return invalid value and hopefully fall back to the second method
                    return 0;
                }
            }
        }

        latency_model.stalls_calibration_factor += step_value;
    } // while

    return best_factor;
}

static double calibrate_with_size(virtual_node_t *virtual_node, double calib_size, int from_node, int to_node) {
	double best_factor;

	// first method decrements the factor with the provided step size
    if (((best_factor = calibrate(virtual_node, (-calib_size), from_node, to_node)) == 0) ||
            calib_size == CALIBRATION_FINEST_STEP) {
        if (best_factor > 0.0) {
        	// recover last best factor
            latency_model.stalls_calibration_factor = best_factor;
        }
        // second method increments the factor with the provided step size
        // this method will be always performed if the provided step size is the finest
        best_factor = calibrate(virtual_node, calib_size, from_node, to_node);
    }

    return best_factor;
}

void latency_calibration(virtual_node_t *virtual_node) {
    double best_factor;
    int from_node = virtual_node->dram_node->node_id;
    int to_node = virtual_node->nvram_node->node_id;

    // if calibration file exist, load the correction factor and exit
    if (calibrate_load_from_file(virtual_node) == E_SUCCESS) {
        return;
    }

    if ((best_factor = calibrate_with_size(virtual_node, CALIBRATION_STEP_SIZE, from_node, to_node)) != 0) {
    	latency_model.stalls_calibration_factor = best_factor + CALIBRATION_FINEST_STEP;
    	best_factor = calibrate_with_size(virtual_node, CALIBRATION_FINEST_STEP, from_node, to_node);
    }

    if (best_factor == 0.0) {
        best_factor = 1.0;
    }

    // set the hardware latency to the best fit value
    latency_model.stalls_calibration_factor = best_factor;
    DBG_LOG(INFO, "CALIBRATION: CPU stalls correction factor is %f (dram %d, nvram %d)\n",
    		best_factor, from_node, to_node);

    // save file for local or remote 'correction factor'
    calibrate_save_to_file(virtual_node, best_factor);
}

#endif // CALIBRATION SUPPORT
