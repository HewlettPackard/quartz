#include "fam_atomic.h"
#include "fam_atomic_model.h"

int64_t global;

void report_latency(void (*foo)())
{
    hrtime_t start = asm_rdtsc();
    foo();
    hrtime_t end = asm_rdtsc();
    printf("latency cycles == %lu\n", end - start);
}

void atomic_fence() 
{
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_atomic_64_compare_store(&global, 0, 1);
}

void atomic_fence8() 
{
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_atomic_64_compare_store(&global, 0, 1);
    fam_atomic_64_compare_store(&global, 0, 1);
}

void fam_copy()
{
    char buf1[4096];
    char buf2[4096];
    fam_memcpy(buf1, buf2, 1024);

}

int main()
{
    fam_fence(); // dummy request to force initialization of quartz
    report_latency(atomic_fence);
    report_latency(atomic_fence);
    report_latency(atomic_fence);
    report_latency(atomic_fence8);
    
    report_latency(fam_copy);
    
}
