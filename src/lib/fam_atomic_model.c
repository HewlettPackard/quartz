#include "fam_atomic_model.h"
#include "config.h"
#include "error.h"
#include "queue.h"


typedef struct {
    queue_t reqs;

} fam_thread_t;

__thread fam_thread_t* fam_tls_thread = NULL;

fam_model_t fam_model; 

int fam_init(config_t* cfg, int cpu_speed_mhz)
{
    __cconfig_lookup_bool(cfg, "fam.invalidate", &fam_model.invalidate_enabled);
    __cconfig_lookup_int(cfg, "fam.invalidate_latency", &fam_model.invalidate_latency);
    __cconfig_lookup_bool(cfg, "fam.persist", &fam_model.persist_enabled);
    __cconfig_lookup_int(cfg, "fam.persist_latency", &fam_model.persist_latency);
    __cconfig_lookup_int(cfg, "fam.atomic_latency", &fam_model.atomic_latency);
    __cconfig_lookup_int(cfg, "fam.atomic_parallelism", &fam_model.atomic_parallelism);

    fam_model.cpu_speed_mhz = cpu_speed_mhz;

    return E_SUCCESS;
}

void fam_atomic_model_wait_all_reqs_complete(hrtime_t now)
{
    if (!fam_tls_thread) {
        fam_tls_thread = malloc(sizeof(*fam_tls_thread));
        queue_init(&fam_tls_thread->reqs, fam_model.atomic_parallelism);
    }

    int reqs_waited = 0; 

    while (!queue_is_empty(&fam_tls_thread->reqs)) 
    {
        hrtime_t oldest;
        queue_front(&fam_tls_thread->reqs, (void**) &oldest);
        hrtime_t target = oldest + ns_to_cycles(fam_model.cpu_speed_mhz, fam_model.atomic_latency);
        if (asm_rdtsc() < target) {
            reqs_waited++;
        }
        while (asm_rdtsc() < target);
        queue_dequeue(&fam_tls_thread->reqs);
    }
}

void fam_atomic_model_wait_available_req_slot(hrtime_t now)
{
    if (!fam_tls_thread) {
        fam_tls_thread = malloc(sizeof(*fam_tls_thread));
        queue_init(&fam_tls_thread->reqs, fam_model.atomic_parallelism);
    }

    if (queue_is_full(&fam_tls_thread->reqs)) 
    {
        hrtime_t oldest;
        queue_front(&fam_tls_thread->reqs, (void**) &oldest);
        hrtime_t target = oldest + ns_to_cycles(fam_model.cpu_speed_mhz, fam_model.atomic_latency);
        while (asm_rdtsc() < target);
        queue_dequeue(&fam_tls_thread->reqs);
    }
}

void fam_atomic_model_queue_enqueue(hrtime_t now)
{
    queue_enqueue(&fam_tls_thread->reqs, (void*) now);
}

void fam_atomic_model_emulate_latency_ns(int ns)
{
    hrtime_t cycles;
    hrtime_t start;
    hrtime_t stop;
    
    start = asm_rdtsc();
    cycles = ns_to_cycles(fam_model.cpu_speed_mhz, ns);

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



