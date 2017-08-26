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

#include "libpthread.h"

#include <assert.h>
#include <dlfcn.h>

int (*libpthread_pthread_create)(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*start_routine) (void *), void *arg);
int (*libpthread_pthread_mutex_lock)(pthread_mutex_t *mutex);
int (*libpthread_pthread_mutex_trylock)(pthread_mutex_t *mutex);
int (*libpthread_pthread_mutex_unlock)(pthread_mutex_t *mutex);
int (*libpthread_pthread_detach)(pthread_t thread);
int (*libpthread_pthread_kill)(pthread_t thread, int sig);
int (*libpthread_pthread_barrier_wait)(pthread_barrier_t *barrier);
int (*libpthread_pthread_barrier_destroy)(pthread_barrier_t *barrier);
int (*libpthread_pthread_join)(pthread_t thread, void **value_ptr);
int (*libpthread_pthread_sigmask)(int how, const sigset_t *set, sigset_t *oldset);
int (*libpthread_pthread_barrier_init)(pthread_barrier_t * barrier, const pthread_barrierattr_t * attr, unsigned count); 

static void libpthread_init() __attribute__((constructor));
static void libthread_finalize() __attribute__((destructor));

static void* libpthread_handle;

void libpthread_init()
{
    libpthread_handle = dlopen("libpthread.so.0", RTLD_NOW);

    assert(libpthread_handle);

    libpthread_pthread_create = dlsym(libpthread_handle, "pthread_create");
    libpthread_pthread_mutex_lock = dlsym(libpthread_handle, "pthread_mutex_lock");
    libpthread_pthread_mutex_trylock = dlsym(libpthread_handle, "pthread_mutex_trylock");
    libpthread_pthread_mutex_unlock = dlsym(libpthread_handle, "pthread_mutex_unlock");
    libpthread_pthread_join = dlsym(libpthread_handle, "pthread_join");
    libpthread_pthread_detach = dlsym(libpthread_handle, "pthread_detach");
    libpthread_pthread_kill = dlsym(libpthread_handle, "pthread_kill");
    libpthread_pthread_barrier_init = dlsym(libpthread_handle, "pthread_barrier_init");
    libpthread_pthread_barrier_wait = dlsym(libpthread_handle, "pthread_barrier_wait");
    libpthread_pthread_barrier_destroy = dlsym(libpthread_handle, "pthread_barrier_destroy");
    libpthread_pthread_sigmask = dlsym(libpthread_handle, "pthread_barrier_sigmask");
}

void libpthread_finalize() 
{
    dlclose(libpthread_handle);
}
