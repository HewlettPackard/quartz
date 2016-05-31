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
#include <sys/syscall.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include "cpu/cpu.h"
#include "utlist.h"
#include "error.h"
#include "interpose.h"
#include "model.h"
#include "thread.h"
#include "topology.h"
#include "monotonic_timer.h"

static thread_manager_t* thread_manager = NULL;
__thread thread_t* tls_thread = NULL;

extern inline hrtime_t hrtime_cycles(void);

// assign a virtual/physical node using a round-robin policy
static void rr_next_cpu_id(thread_manager_t* thread_manager, int* next_virtual_node_idp, int* next_cpu_idp)
{
    int next_virtual_node_id;
    virtual_node_t* virtual_node;
    physical_node_t* physical_node;
    virtual_topology_t* virtual_topology = thread_manager->virtual_topology;

    *next_virtual_node_idp = thread_manager->next_virtual_node_id;
    *next_cpu_idp = thread_manager->next_cpu_id;

    // advance to the next virtual node and cpu id
    next_virtual_node_id = thread_manager->next_virtual_node_id;
    virtual_node = &virtual_topology->virtual_nodes[next_virtual_node_id];
    physical_node = virtual_node->dram_node; // we run threads on the dram node
    if ((thread_manager->next_cpu_id = next_cpu(physical_node->cpu_bitmask, thread_manager->next_cpu_id + 1)) < 0) {
        next_virtual_node_id = (next_virtual_node_id + 1) % virtual_topology->num_virtual_nodes;
        virtual_node = &virtual_topology->virtual_nodes[next_virtual_node_id];
        physical_node = virtual_node->dram_node;
        thread_manager->next_cpu_id = first_cpu(physical_node->cpu_bitmask);
        thread_manager->next_virtual_node_id = next_virtual_node_id;
    } 
}

void rr_set_next_cpu_based_on_rank(int rank, int max_rank)
{
    int cpu_id;
    int virtual_node_id;
    int i;

    // set the next CPU id based on this process rank id
    thread_manager->next_virtual_node_id = 0;
    thread_manager->next_cpu_id = 0;
    for (i = 0; i <= rank; ++i) {
        rr_next_cpu_id(thread_manager, &virtual_node_id, &cpu_id);
    }

    DBG_LOG(DEBUG, "no partitioning of CPUs, set next CPU "
                   "to vnode %d and cpu %d\n", virtual_node_id, cpu_id);
}

void partition_cpus_based_on_rank(int rank, int max_rank, int num_cpus,
                                  virtual_topology_t* virtual_topology)
{
    // assumed the number of cpus/2 is greater or equal to max_rank
    // this partition is num_cpus/max_rank
    int part_size = num_cpus/max_rank;
    int start = rank * part_size;
    int end = start + part_size -1;
    int i;
    int cpu_id = 0;
    int virtual_node_id = 0;
    virtual_node_t* virtual_node;
    physical_node_t* physical_node;

    DBG_LOG(DEBUG, "partitioning CPUS, this process has CPUs from %d and %d\n",
            start, end);

    thread_manager->next_virtual_node_id = 0;
    thread_manager->next_cpu_id = 0;
    for (i = 0; i < num_cpus; ++i) {
        rr_next_cpu_id(thread_manager, &virtual_node_id, &cpu_id);
        if (i < start || i > end) {
            // this CPU is outside the partition of this process
            // disable this CPU
            virtual_node = &virtual_topology->virtual_nodes[virtual_node_id];
            physical_node = virtual_node->dram_node;

            DBG_LOG(DEBUG, "disabling CPU %d\n", cpu_id);

            if (numa_bitmask_isbitset(physical_node->cpu_bitmask, cpu_id)) {
                numa_bitmask_clearbit(physical_node->cpu_bitmask, cpu_id);
            }
        }
    }
}

int bind_thread_on_cpu(thread_manager_t* thread_manager, thread_t* thread, int virtual_node_id, int cpu_id)
{
    thread->virtual_node = &thread_manager->virtual_topology->virtual_nodes[virtual_node_id];
    DBG_LOG(INFO, "Binding thread tid [%d] pthread: 0x%lx on processor %d\n", thread->tid, thread->pthread, cpu_id);
    struct bitmask* cpubind = numa_allocate_cpumask();
    numa_bitmask_setbit(cpubind, cpu_id);
    if (numa_sched_setaffinity(thread->tid, cpubind) != 0) {
        DBG_LOG(ERROR, "Cannot bind thread tid [%d] pthread: 0x%lx on processor %d\n", thread->tid, thread->pthread, cpu_id);
        numa_bitmask_free(cpubind);
        return E_ERROR;
    }
    numa_bitmask_free(cpubind);
    return E_SUCCESS;
}

int bind_thread_on_mem(thread_manager_t* thread_manager, thread_t* thread, int virtual_node_id, int cpu_id)
{
    int physical_node_id;
    struct bitmask* membind = numa_allocate_nodemask();
    physical_node_id = thread_manager->virtual_topology->virtual_nodes[virtual_node_id].dram_node->node_id;
    numa_bitmask_setbit(membind, physical_node_id);
    numa_set_membind(membind);
    numa_free_nodemask(membind);

    return E_SUCCESS;
}

thread_t* thread_self()
{
    return tls_thread;
}

void thread_interrupt_handler(int signum)
{
    DBG_LOG(DEBUG, "Handling interrupt thread [%d] pthread: 0x%lx\n", thread_self()->tid, thread_self()->pthread);

    create_latency_epoch();
}

#ifdef PAPI_SUPPORT
static int setup_events_thread_self(thread_t *thread, const char **native_events) {
    int i;

    // create event set for this thread
    if (pmc_create_event_set_local_thread() != 0) {
       return -1;
    }

    // register events for this thread
    for (i = 0; i < MAX_NUM_EVENTS; ++i) {
   	    if (native_events[i]) {
            DBG_LOG(INFO, "registering event %s, thread id [%d]\n", native_events[i], thread->tid);
            if (pmc_register_event_local_thread(native_events[i]) != 0) {
                return E_ERROR;
            }
        }
    }

    // start event counting for this thread
    if (pmc_events_start_local_thread() != 0) {
    	return E_ERROR;
    }

    pmc_register_thread();

    return 0;
}
#endif

int register_thread(thread_manager_t* thread_manager, pthread_t pthread, pid_t tid)
{
    int ret = 0;
    int cpu_id;
    int virtual_node_id;
    thread_t* thread = malloc(sizeof(thread_t));

    if (thread_manager == NULL) {
        // this is possible if both BW and latency modeling are enabled and the BW model is not yet created.
        // the BW modeling will spawn threads which will attempt to register with the thread manager if the
        // latency modeling is enabled. However the thread manager is instantiated later.
        //goto error;
        return E_SUCCESS;
    }

    memset(thread, 0, sizeof(thread_t));

    thread->pthread = pthread;
    thread->tid = tid;
    thread->thread_manager = thread_manager;

#ifdef USE_STATISTICS
    if (thread_manager->stats.enabled) {
        thread->stats.last_epoch_timestamp = monotonic_time_us();
        thread->stats.shortest_epoch_duration_us = UINT64_MAX;
    }
#endif

	/* install thread interrupt handler as the signal handler for SIGUSR1. */
    struct sigaction sa;
    memset (&sa, 0, sizeof(sa));
    sa.sa_handler = &thread_interrupt_handler;
    sa.sa_flags = SA_RESTART;
    sigaction (SIGUSR1, &sa, NULL);

    // bind the thread on a cpu and memory node and
    // link the thread to the list of threads
    assert(__lib_pthread_mutex_lock);
    __lib_pthread_mutex_lock(&thread_manager->mutex);
    rr_next_cpu_id(thread_manager, &virtual_node_id, &cpu_id);
    if ((ret = bind_thread_on_cpu(thread_manager, thread, virtual_node_id, cpu_id)) != E_SUCCESS) {
    	__lib_pthread_mutex_unlock(&thread_manager->mutex);
    	DBG_LOG(ERROR, "thread id [%d] failed to bind to CPU\n", thread->tid);
        goto error;
    }
    if ((ret = bind_thread_on_mem(thread_manager, thread, virtual_node_id, cpu_id)) != E_SUCCESS) {
    	__lib_pthread_mutex_unlock(&thread_manager->mutex);
    	DBG_LOG(ERROR, "thread id [%d] failed to bind to Memory\n", thread->tid);
        goto error;
    }
    thread->cpu_id = cpu_id;
    thread->cpu_speed_mhz = cpu_speed_mhz();
#ifdef PAPI_SUPPORT
    cpu_model_t *cpu = thread_manager->virtual_topology->virtual_nodes[virtual_node_id].dram_node->cpu_model;
    if (setup_events_thread_self(thread, cpu->pmc_events.native_events) != 0) {
        ret = E_ERROR;
        __lib_pthread_mutex_unlock(&thread_manager->mutex);
        goto error;
    }
#endif
    LL_APPEND(thread_manager->thread_list, thread);
#ifdef USE_STATISTICS
    if (thread_manager->stats.enabled) {
        thread_manager->stats.n_threads++;
        thread->stats.register_timestamp = monotonic_time_us();
    }
#endif
    __lib_pthread_mutex_unlock(&thread_manager->mutex);

    init_thread_latency_model(thread);

    tls_thread = thread;

    return E_SUCCESS;

error:
    free(thread);
    DBG_LOG(ERROR, "thread id [%d] failed to register with Monitor Thread\n", thread->tid);
    return ret;
}


int unregister_thread(thread_manager_t* thread_manager, thread_t * thread)
{
    __lib_pthread_mutex_lock(&thread_manager->mutex);

    if (thread_manager == NULL) {
        return E_SUCCESS;
    }

    LL_DELETE(thread_manager->thread_list, thread);

#ifdef USE_STATISTICS
    if (thread_manager->stats.enabled) {
        thread->stats.unregister_timestamp = monotonic_time_us();
        LL_APPEND(thread_manager->stats.thread_list, thread);
    }
#endif

    __lib_pthread_mutex_unlock(&thread_manager->mutex);

#ifdef PAPI_SUPPORT
    pmc_events_stop_local_thread();
    pmc_destroy_event_set_local_thread();
    pmc_unregister_thread();
#endif

    return E_SUCCESS;
}


int register_self()
{
	int ret = E_SUCCESS;

    if (thread_self() == NULL) {
    	pid_t tid = (pid_t) syscall(SYS_gettid);
    	DBG_LOG(INFO, "Registering thread tid [%d]\n", tid);
        ret = register_thread(thread_manager, pthread_self(), tid);
    }

    return ret;
}

int unregister_self()
{
	if (tls_thread) {
	    unregister_thread(thread_manager, tls_thread);

#ifdef USE_STATISTICS
	    if (!thread_manager->stats.enabled) {
		    // statistics makes use of the thread descriptor
            free(tls_thread);
	    }
#else
	    free(tls_thread);
#endif
        tls_thread = NULL;
	}

    return E_SUCCESS;
}

static int reached_max_epoch_duration(thread_t* thread);
void interrupt_threads(thread_manager_t* manager)
{
    thread_t* thread;

    assert(__lib_pthread_mutex_lock);
    __lib_pthread_mutex_lock(&manager->mutex);
    LL_FOREACH(manager->thread_list, thread)
    {
    	assert(thread);
        if (thread->signaled == 0 && reached_max_epoch_duration(thread)) {
            DBG_LOG(DEBUG, "interrupting thread [%d]\n", thread->tid);
#ifdef USE_STATISTICS
            if (manager->stats.enabled) {
                thread->stats.signals_sent++;
            }
#endif
            // this flag must be set before the signal is sent to make sure
            // there will be no race condition
            thread->signaled = 1;
            pthread_kill(thread->pthread, SIGUSR1);
        }
    }
    assert(__lib_pthread_mutex_unlock);
    __lib_pthread_mutex_unlock(&manager->mutex);
}

void* monitor_thread(void* arg)
{
    thread_manager_t* manager = (thread_manager_t*) arg;
    struct timespec epoch_duration;
//    time_t secs = thread_manager->max_epoch_duration_us / USECS_PER_SEC;
//    long nanosecs = (thread_manager->max_epoch_duration_us % USECS_PER_SEC) * NANOS_PER_USEC;

    epoch_duration.tv_sec = 0;
    epoch_duration.tv_nsec = MIN_EPOCH_DURATION_US * 1000;
    while(1) {
        nanosleep(&epoch_duration, NULL);
        interrupt_threads(manager);
    }
    return NULL;
}

static void set_epoch_duration(config_t* cfg, const char *config_str, int *epoch_us, int default_epoch_us) {
    if (__cconfig_lookup_int(cfg, config_str, epoch_us) != CONFIG_TRUE) {
    	*epoch_us = default_epoch_us;
    } else {
        if (*epoch_us > MAX_EPOCH_DURATION_US ||
                *epoch_us < MIN_EPOCH_DURATION_US) {
            DBG_LOG(WARNING, "%s is out of supported bounds [%i, %i], setting it to %i\n",
            		config_str,
            		MIN_EPOCH_DURATION_US,
            		MAX_EPOCH_DURATION_US,
					default_epoch_us);
            *epoch_us = default_epoch_us;
        }
    }
}

int init_thread_manager(config_t* cfg, virtual_topology_t* virtual_topology)
{
    int ret;
    pthread_t monitor_tid;
    thread_manager_t* mgr;
    virtual_node_t* virtual_node;
    physical_node_t* physical_node;

    if (!(mgr = malloc(sizeof(thread_manager_t)))) {
        ret = E_ERROR;
        goto done;    
    }

    memset(mgr, 0, sizeof(thread_manager_t));

    mgr->thread_list = NULL;
    mgr->virtual_topology = virtual_topology;
    mgr->next_virtual_node_id = 0;

    set_epoch_duration(cfg, "latency.max_epoch_duration_us", &mgr->max_epoch_duration_us, MAX_EPOCH_DURATION_US);
    set_epoch_duration(cfg, "latency.min_epoch_duration_us", &mgr->min_epoch_duration_us, MIN_EPOCH_DURATION_US);

    if (mgr->min_epoch_duration_us > mgr->max_epoch_duration_us) {
        DBG_LOG(WARNING, "latency.min_epoch_duration_us is greater than latency.max_epoch_duration_us, setting it to %i\n",
                MIN_EPOCH_DURATION_US);
        mgr->min_epoch_duration_us = MIN_EPOCH_DURATION_US;
    }

    virtual_node = &virtual_topology->virtual_nodes[mgr->next_virtual_node_id];
    physical_node = virtual_node->dram_node;
    mgr->next_cpu_id = first_cpu(physical_node->cpu_bitmask);
    pthread_mutex_init(&mgr->mutex, NULL);

    // fire a monitoring thread that periodically interrupts threads
    assert(__lib_pthread_create);
    assert(__lib_pthread_detach);
    __lib_pthread_create(&monitor_tid, NULL, monitor_thread, (void*) mgr);
    __lib_pthread_detach(monitor_tid);

    thread_manager = mgr;
    return E_SUCCESS;

done:
    return ret;
}

int reached_min_epoch_duration(thread_t* thread) {
	double current_time;
	uint64_t diff_us;
	int result = 0;

    if (thread == NULL) {
    	// FIXME: JVM for instance create threads using a mechanism not traced by this emulator
    	//        for now we make sure the current thread is registered right when it makes the
    	//        first explicit NVM allocation or when interposed functions are called. A
    	//        better solution is to trace the thread creation done by JVM.
        if (register_self() != E_SUCCESS)
        	// if the thread could not be registered, exit this function
        	return 0;
        thread = thread_self();
    }

	current_time = monotonic_time_us();

#ifdef USE_STATISTICS
    diff_us = (uint64_t) (current_time - thread->stats.last_epoch_timestamp);
#else
    diff_us = (uint64_t) (current_time - thread->last_epoch_timestamp);
#endif

    DBG_LOG(DEBUG, "thread id [%d] last epoch was %lu usec ago\n", thread->tid, diff_us);

    if(diff_us >= thread_manager->min_epoch_duration_us) {
    	DBG_LOG(DEBUG, "thread id [%d] reached min epoch duration (%i usec)\n", thread->tid,
    			thread_manager->min_epoch_duration_us);
        result = 1;
    }
#ifdef USE_STATISTICS
    if (thread_manager->stats.enabled && ! result) {
    	thread->stats.min_epoch_not_reached++;
    }
#endif
    return result;
}

static int reached_max_epoch_duration(thread_t* thread) {
	double current_time;
	uint64_t diff_us;
	int result = 0;

	// it compares this time with the last_epoch_timestamp, which is set by another thread
	// so, this time must be based on a system time and not on CPU cycles/time registers
	current_time = monotonic_time_us();

#ifdef USE_STATISTICS
    diff_us = (uint64_t) (current_time - thread->stats.last_epoch_timestamp);
#else
    diff_us = (uint64_t) (current_time - thread->last_epoch_timestamp);
#endif

    DBG_LOG(DEBUG, "thread id [%d] last epoch was %lu usec ago\n", thread->tid, diff_us);

    if(diff_us >= thread_manager->max_epoch_duration_us) {
    	DBG_LOG(DEBUG, "thread id [%d] reached max epoch duration (%i usec)\n", thread->tid,
    			thread_manager->max_epoch_duration_us);
        result = 1;
    }

    return result;
}

void block_new_epoch() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
}

void unblock_new_epoch() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

thread_manager_t* get_thread_manager() {
	return thread_manager;
}
