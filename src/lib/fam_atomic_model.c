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
    __cconfig_lookup_bool(cfg, "fam.enable", &fam_model.enabled);
    __cconfig_lookup_int(cfg, "fam.parallelism", &fam_model.parallelism);
    __cconfig_lookup_int(cfg, "fam.invalidate_latency", &fam_model.invalidate_latency);
    __cconfig_lookup_int(cfg, "fam.persist_latency", &fam_model.persist_latency);
    __cconfig_lookup_int(cfg, "fam.read_latency", &fam_model.read_latency);
    __cconfig_lookup_int(cfg, "fam.atomic_latency", &fam_model.atomic_latency);

    fam_model.cpu_speed_mhz = cpu_speed_mhz;

    return E_SUCCESS;
}

void fam_atomic_model_wait_all_reqs_complete(hrtime_t now)
{
    if (!fam_tls_thread) {
        fam_tls_thread = malloc(sizeof(*fam_tls_thread));
        queue_init(&fam_tls_thread->reqs, fam_model.parallelism);
    }

    int reqs_waited = 0; 

    while (!queue_is_empty(&fam_tls_thread->reqs)) 
    {
        hrtime_t oldest;
        queue_front(&fam_tls_thread->reqs, (void**) &oldest);
        if (asm_rdtsc() < oldest) {
            reqs_waited++;
        }
        while (asm_rdtsc() < oldest);
        queue_dequeue(&fam_tls_thread->reqs);
    }
}

void fam_atomic_model_wait_available_req_slot(hrtime_t now)
{
    if (!fam_tls_thread) {
        fam_tls_thread = malloc(sizeof(*fam_tls_thread));
        queue_init(&fam_tls_thread->reqs, fam_model.parallelism);
    }

    if (queue_is_full(&fam_tls_thread->reqs)) 
    {
        hrtime_t oldest;
        queue_front(&fam_tls_thread->reqs, (void**) &oldest);
        while (asm_rdtsc() < oldest);
        queue_dequeue(&fam_tls_thread->reqs);
    }
}

void fam_atomic_model_queue_enqueue_request(hrtime_t now_cycles, int latency_cycles)
{
    hrtime_t target = now_cycles + latency_cycles;
    queue_enqueue(&fam_tls_thread->reqs, (void*) target);
}

void fam_atomic_model_queue_enqueue_request_ns(hrtime_t now_cycles, int latency_ns)
{
    int latency_cycles = ns_to_cycles(fam_model.cpu_speed_mhz, latency_ns);
    fam_atomic_model_queue_enqueue_request(now_cycles, latency_cycles);
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

/*
 * Certain operations invoking this method, such as memcpy, may take a finite 
 * amount of time to complete, so we want to include that time into the emulation
 * as overlapping cycles to avoid double accounting.
 */
void fam_atomic_model_range_access(size_t len, int request_latency_ns, int overlap_cycles)
{
    int i;
    int request_latency_cycles = ns_to_cycles(fam_model.cpu_speed_mhz, request_latency_ns);
    for (i=0; i < len; i+=64) {
        hrtime_t now_cycles = asm_rdtsc();
        fam_atomic_model_wait_available_req_slot(now_cycles);
        fam_atomic_model_queue_enqueue_request((void*) (now_cycles-overlap_cycles), request_latency_cycles);
    }
    hrtime_t now_cycles = asm_rdtsc();
    fam_atomic_model_wait_all_reqs_complete(now_cycles);
}
