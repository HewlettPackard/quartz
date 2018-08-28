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
#include "pflush.h"

#include <stdint.h>

typedef uint64_t hrtime_t;

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

/* Flush cacheline */
#define asm_clflush(addr)                   \
({                              \
    __asm__ __volatile__ ("clflush %0" : : "m"(*addr)); \
})

/* Memory fence */
#define asm_mfence()                \
({                      \
    PM_FENCE();             \
    __asm__ __volatile__ ("mfence");    \
})

static int global_cpu_speed_mhz = 0;
static int global_write_latency_ns = 0;

void init_pflush(int cpu_speed_mhz, int write_latency_ns)
{
    global_cpu_speed_mhz = cpu_speed_mhz;
    global_write_latency_ns = write_latency_ns;
}

inline hrtime_t cycles_to_ns(int cpu_speed_mhz, hrtime_t cycles)
{
    return (cycles*1000/cpu_speed_mhz);
}

inline hrtime_t ns_to_cycles(int cpu_speed_mhz, hrtime_t ns)
{
    return (ns*cpu_speed_mhz/1000);
}

static inline
void
emulate_latency_ns(int ns)
{
    hrtime_t cycles;
    hrtime_t start;
    hrtime_t stop;
    
    start = asm_rdtsc();
    cycles = ns_to_cycles(global_cpu_speed_mhz, ns);

    do { 
        /* RDTSC doesn't necessarily wait for previous instructions to complete 
         * so a serializing instruction is usually used to ensure previous 
         * instructions have completed. However, in our case this is a desirable
         * property since we want to overlap the latency we emulate with the
         * actual latency of the emulated instruction. 
         */
        stop = asm_rdtsc();
    } while (stop - start < cycles);
}

void
pflush(uint64_t *addr)
{
    if (global_write_latency_ns == 0) {
        return;
    }

    /* Measure the latency of a clflush and add an additional delay to
     * meet the latency to write to NVM */
    hrtime_t start;
    hrtime_t stop;
    start = asm_rdtscp();
    asm_clflush(addr);  
    stop = asm_rdtscp();
    int to_insert_ns = global_write_latency_ns - cycles_to_ns(global_cpu_speed_mhz, stop-start);
    if (to_insert_ns <= 0) {
        return;
    }
    emulate_latency_ns(to_insert_ns);
}
