#include "libpthread.h"

#include <dlfcn.h>

int (*libpthread_pthread_create)(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*start_routine) (void *), void *arg);
int (*libpthread_pthread_mutex_lock)(pthread_mutex_t *mutex);
int (*libpthread_pthread_mutex_trylock)(pthread_mutex_t *mutex);
int (*libpthread_pthread_mutex_unlock)(pthread_mutex_t *mutex);
int (*libpthread_pthread_detach)(pthread_t thread);

static void libpthread_init() __attribute__((constructor));
static void libthread_finalize() __attribute__((destructor));

static void* libpthread_handle;

void libpthread_init()
{
    libpthread_handle = dlopen("libpthread.so", RTLD_NOW);

    libpthread_pthread_create = dlsym(libpthread_handle, "pthread_create");
    libpthread_pthread_mutex_lock = dlsym(libpthread_handle, "pthread_mutex_lock");
    libpthread_pthread_mutex_trylock = dlsym(libpthread_handle, "pthread_mutex_trylock");
    libpthread_pthread_mutex_unlock = dlsym(libpthread_handle, "pthread_mutex_unlock");
    libpthread_pthread_detach = dlsym(libpthread_handle, "pthread_detach");
}

void libpthread_finalize() 
{
    dlclose(libpthread_handle);
}
