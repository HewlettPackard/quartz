// Copyright 2013 Alex Reece.
//
// A cross platform monotonic timer.

#include <unistd.h>
#include "monotonic_timer.h"

#if _POSIX_TIMERS > 0 && defined(_POSIX_MONOTONIC_CLOCK)
  // If we have it, use clock_gettime and CLOCK_MONOTONIC.

  #include <time.h>

  double monotonic_time() {
    struct timespec time;
    // Note: Make sure to link with -lrt to define clock_gettime.
    clock_gettime(CLOCK_MONOTONIC, &time);
    return ((double) time.tv_sec) + ((double) time.tv_nsec / (NANOS_PER_SECF));
  }

  double monotonic_time_us() {
	  struct timespec time;
	  // Note: Make sure to link with -lrt to define clock_gettime.
	  clock_gettime(CLOCK_MONOTONIC, &time);
	  return ((double) (time.tv_sec * USECS_PER_SEC)) + ((double) time.tv_nsec / NANOS_PER_USECF);
  }

#else
  // Fall back to rdtsc. The reason we don't use clock() is this scary message
  // from the man page:
  //     "On several other implementations, the value returned by clock() also
  //      includes the times of any children whose status has been collected via
  //      wait(2) (or another wait-type call)."
  //
  // Also, clock() only has microsecond accuracy.
  //
  // This whitepaper offered excellent advice on how to use rdtscp for
  // profiling: http://download.intel.com/embedded/software/IA/324264.pdf
  //
  // Unfortunately, we can't follow its advice exactly with our semantics,
  // so we're just going to use rdtscp with cpuid.
  //
  // Note that rdtscp will only be available on new processors.

  #include <stdint.h>

  static inline uint64_t rdtsc() {
    uint32_t hi, lo;
    asm volatile("rdtscp\n"
                 "movl %%edx, %0\n"
                 "movl %%eax, %1\n"
                 "cpuid"
                 : "=r" (hi), "=r" (lo) : : "%rax", "%rbx", "%rcx", "%rdx");
    return (((uint64_t)hi) << 32) | (uint64_t)lo;
  }

  static uint64_t rdtsc_per_sec = 0;
  static uint64_t rdtsc_per_usec = 0;
  static void __attribute__((constructor)) init_rdtsc_per_sec() {
    uint64_t before, after;

    before = rdtsc();
    usleep(USECS_PER_SEC);
    after = rdtsc();

    rdtsc_per_sec = after - before;

    before = rdtsc();
    usleep(1);
    after = rdtsc();

    rdtsc_per_usec = after - before;
  }

  double monotonic_time() {
    return (double) rdtsc() / (double) rdtsc_per_sec;
  }

  // TODO: not tested, it is core specific and callers must be aware
  double monotonic_time_us() {
    return ((double) rdtsc() / (double) rdtsc_per_usec);
  }

#endif
