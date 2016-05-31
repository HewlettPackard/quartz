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
#include <stdio.h>
#include <dlfcn.h>
#include <pthread.h>
#include <assert.h>
#include <signal.h>
#include "error.h"
#include "model.h"
#include "thread.h"
#include "cpu/cpu.h"
#ifdef PAPI_SUPPORT
#include "cpu/pmc-papi.h"
#else
#include "cpu/pmc.h"
#endif


// WARNING: Our library MUST directly use the functions we interpose on by 
// calling __lib_X to avoid interposition on ourselves.


int (*__lib_pthread_create)(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*start_routine) (void *), void *arg);
int (*__lib_pthread_mutex_lock)(pthread_mutex_t *mutex);
int (*__lib_pthread_mutex_trylock)(pthread_mutex_t *mutex);
int (*__lib_pthread_mutex_unlock)(pthread_mutex_t *mutex);
int (*__lib_pthread_detach)(pthread_t thread);

extern inline hrtime_t hrtime_cycles(void);
extern inline int cycles_to_us(cpu_model_t* cpu, hrtime_t cycles);


int init_interposition()
{
	char *error;
    // if no symbol is returned then no interposition needed
    __lib_pthread_create = dlsym(RTLD_NEXT, "pthread_create");
    __lib_pthread_mutex_lock = dlsym(RTLD_NEXT, "pthread_mutex_lock");
    __lib_pthread_mutex_trylock = dlsym(RTLD_NEXT, "pthread_mutex_trylock");
    __lib_pthread_mutex_unlock = dlsym(RTLD_NEXT, "pthread_mutex_unlock");
    __lib_pthread_detach = dlsym(RTLD_NEXT, "pthread_detach");

    if (__lib_pthread_mutex_lock == NULL || __lib_pthread_mutex_unlock == NULL ||
    	    __lib_pthread_create == NULL || __lib_pthread_mutex_trylock == NULL ||
    	    __lib_pthread_detach == NULL) {
    	error = dlerror();
    	DBG_LOG(ERROR, "Interposition failed: %s\n", error != NULL ? error : "unknown reason");
    	return E_ERROR;
    }

    return E_SUCCESS;
}


// Interposing on pthread_create requires interposing on the thread created as we 
// require the TID of that thread which we can only get by executing the gettid() 
// system call from that thread. So we interpose on the start_routine which is
// called by the new thread
typedef struct {
    void *(*start_routine) (void *);
    void *arg;
} pthread_create_functor_t;

void* __interposed_start_routine(void* args)
{
    void* ret;
    pthread_create_functor_t* f = (pthread_create_functor_t*) args;
    if (register_self() != E_SUCCESS) {
        free(args);
        return NULL;
    }
    ret = f->start_routine(f->arg);
    // FIXME: directly calling unregister may miss cases where the 
    // thread terminates prematurely (such as pthread_exit or cancel)
    // consider using a key destructor function instead
    //fprintf(stderr, "stall cycles: %lu\n", thread_self()->stall_cycles);
    //fprintf(stderr, "signals_sent: %lu signals_recv: %lu\n", thread_self()->signals_sent, thread_self()->signals_recv);
    unregister_self();
    free(args);
    return ret;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg)
{
    int ret;

    //DBG_LOG(DEBUG, "interposing pthread_create\n");

    //assert(__lib_pthread_create);
    if (__lib_pthread_create == NULL)
        init_interposition();

    if (latency_model.enabled) {
        pthread_create_functor_t *functor = malloc(sizeof(pthread_create_functor_t));
        functor->arg = arg;
        functor->start_routine = start_routine;

        if ((ret = __lib_pthread_create(thread, attr, __interposed_start_routine, (void*) functor)) != 0) {
            DBG_LOG(ERROR, "call to __lib_pthread_create failed\n");
            return ret;
        }
    } else {
        ret = __lib_pthread_create(thread, attr, start_routine, arg);
    }

    return ret;    
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    int err;

    if (latency_model.enabled) {
        if(reached_min_epoch_duration(thread_self())) {
            // create new epoch here in order to propagate only the critical session delay to other threads
            // the thread monitor will keep trying to create new epoch, unless the min duration has not been reached
            create_latency_epoch();
        }
    }

    //DBG_LOG(DEBUG, "interposing pthread_mutex_lock\n");

    //assert(__lib_pthread_mutex_lock);
    if (__lib_pthread_mutex_lock == NULL)
        init_interposition();
    err =  __lib_pthread_mutex_lock(mutex);

    return err;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    int err;

    if (latency_model.enabled) {
        if(reached_min_epoch_duration(thread_self())) {
            create_latency_epoch();
        }
    }

    //DBG_LOG(DEBUG, "interposing pthread_mutex_trylock\n");

    //assert(__lib_pthread_mutex_trylock);
    if (__lib_pthread_mutex_trylock == NULL)
        init_interposition();
    err =  __lib_pthread_mutex_trylock(mutex);

    return err;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    int err;

    if (latency_model.enabled) {
        if (reached_min_epoch_duration(thread_self())) {
            create_latency_epoch();
        }
    }

    //DBG_LOG(DEBUG, "interposing pthread_mutex_unlock\n");

    //assert(__lib_pthread_mutex_unlock);
    if (__lib_pthread_mutex_unlock == NULL)
        init_interposition();
    err = __lib_pthread_mutex_unlock(mutex);

    return err;
}
