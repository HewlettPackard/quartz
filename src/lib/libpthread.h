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

/**
 * \file
 * 
 * Wrapper symbols to invoke libpthread methods from within the quartz 
 * library without being interposed by ourselves.
 */

#ifndef __LIBPTHREAD_H
#define __LIBPTHREAD_H

#include <pthread.h>
#include <signal.h>

extern int (*libpthread_pthread_create)(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*start_routine) (void *), void *arg);
extern int (*libpthread_pthread_mutex_lock)(pthread_mutex_t *mutex);
extern int (*libpthread_pthread_mutex_trylock)(pthread_mutex_t *mutex);
extern int (*libpthread_pthread_mutex_unlock)(pthread_mutex_t *mutex);
extern int (*libpthread_pthread_detach)(pthread_t thread);
extern int (*libpthread_pthread_kill)(pthread_t thread, int sig);
extern int (*libpthread_pthread_barrier_init)(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned count); 
extern int (*libpthread_pthread_barrier_wait)(pthread_barrier_t *barrier);
extern int (*libpthread_pthread_barrier_destroy)(pthread_barrier_t *barrier);
extern int (*libpthread_pthread_join)(pthread_t thread, void **value_ptr);
extern int (*libpthread_pthread_sigmask)(int how, const sigset_t *set, sigset_t *oldset);

#endif // __LIBPTHREAD_H
