/*
 * Copyright 2016 Hewlett Packard Enterprise Development LP.
 *
 * This program is free software; you can redistribute it and.or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version. This program is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details. You
 * should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __FAM_ATOMIC_MODEL_H
#define __FAM_ATOMIC_MODEL_H

#include <stdint.h>
#include "config.h"

typedef struct {
    int invalidate_enabled;
    int invalidate_latency;
    int persist_enabled;
    int persist_latency;
    int atomic_latency;
    int atomic_parallelism;
    int cpu_speed_mhz;
} fam_model_t;

typedef uint64_t hrtime_t;

extern fam_model_t fam_model; 

int fam_init(config_t* cfg, int cpu_speed_mhz);
void fam_atomic_model_emulate_latency_ns(int ns);
void fam_atomic_model_wait_all_reqs_complete(hrtime_t now);
void fam_atomic_model_wait_available_req_slot(hrtime_t now);
void fam_atomic_model_queue_enqueue(hrtime_t now);


#if defined(__i386__)

static inline unsigned long long asm_rdtsc(void)
{
    unsigned long long int x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}

static inline unsigned long long asm_rdtscp(void)
{
        unsigned hi, lo;
    __asm__ __volatile__ ("rdtscp" : "=a"(lo), "=d"(hi)::"ecx");
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );

}
#elif defined(__x86_64__)

static inline unsigned long long asm_rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

static inline unsigned long long asm_rdtscp(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtscp" : "=a"(lo), "=d"(hi)::"rcx");
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#else
#error "What architecture is this???"
#endif

static inline hrtime_t cycles_to_ns(int cpu_speed_mhz, hrtime_t cycles)
{
    return (cycles*1000/cpu_speed_mhz);
}

static inline hrtime_t ns_to_cycles(int cpu_speed_mhz, hrtime_t ns)
{
    return (ns*cpu_speed_mhz/1000);
}

#endif // __FAM_ATOMIC_MODEL_H
