/**
 * \file
 * 
 * Wrapper symbols to invoke libpthread methods from within the quartz 
 * library without being interposed by ourselves.
 */

#ifndef __LIBPTHREAD_H
#define __LIBPTHREAD_H

#include <pthread.h>

extern int (*libpthread_pthread_create)(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*start_routine) (void *), void *arg);
extern int (*libpthread_pthread_mutex_lock)(pthread_mutex_t *mutex);
extern int (*libpthread_pthread_mutex_trylock)(pthread_mutex_t *mutex);
extern int (*libpthread_pthread_mutex_unlock)(pthread_mutex_t *mutex);
extern int (*libpthread_pthread_detach)(pthread_t thread);

#endif // __LIBPTHREAD_H
