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
#include <papi.h>
#include <pthread.h>
#include <sys/syscall.h>
#include "cpu/pmc-papi.h"
#include "debug.h"

__thread int tls_event_set = PAPI_NULL;

#define STR_MAX_SIZE 256

static void log_papi_critical(int ret_val, const char *msg) {
	//char papi_str[STR_MAX_SIZE];
	//PAPI_perror(ret_val, (char *)papi_str, sizeof(papi_str));
    DBG_LOG(CRITICAL, "%s (%s)\n", msg, PAPI_strerror(ret_val));
}

int pmc_init() {
	int ret_val;

    if ((ret_val = PAPI_library_init(PAPI_VER_CURRENT)) != PAPI_VER_CURRENT) {
        log_papi_critical(ret_val, "PMC library init error");
        return -1;
    }

    if ((ret_val = PAPI_thread_init(pthread_self)) != PAPI_OK) {
        log_papi_critical(ret_val, "PMC thread support init error");
        return -1;
    }

//    if ((ret_val = PAPI_set_domain(PAPI_DOM_ALL)) != PAPI_OK) {
//        log_papi_critical(ret_val, "PMC set domain error");
//        return -1;
//    }

    return 0;
}

void pmc_shutdown() {
    PAPI_shutdown();
}

int pmc_create_event_set_local_thread() {
	int ret_val;

    if ((ret_val = PAPI_create_eventset(&tls_event_set)) != PAPI_OK) {
        log_papi_critical(ret_val, "PMC event set init error");
        return -1;
    }

//    if ((ret_val = PAPI_set_granularity(PAPI_GRN_SYS)) != PAPI_OK) {
//        log_papi_critical(ret_val, "PMC set granularity error");
//        return -1;
//    }

    return 0;
}

void pmc_destroy_event_set_local_thread() {
    PAPI_cleanup_eventset(tls_event_set);
    PAPI_destroy_eventset(&tls_event_set);
}

int pmc_register_thread() {
	return PAPI_register_thread();
}

int pmc_unregister_thread() {
	return PAPI_unregister_thread();
}

int pmc_register_event_local_thread(const char *event_name) {
    int ret_val;
    char msg[STR_MAX_SIZE];

    // The pthread scope for each thread should be set to PTHREAD_SCOPE_SYSTEM.
    // On linux, pthread supports only PTHREAD_SCOPE_SYSTEM.

    assert(tls_event_set != PAPI_NULL);
    assert(event_name);

    if ((ret_val = PAPI_add_named_event(tls_event_set, (char *)event_name)) != PAPI_OK) {
    	snprintf(msg, sizeof(msg), "PMC event (%s) register error", event_name);
    	log_papi_critical(ret_val, msg);
        return -1;
    }

    return 0;
}

int pmc_events_start_local_thread() {
    int ret_val;

    assert(tls_event_set != PAPI_NULL);

    if ((ret_val = PAPI_start(tls_event_set)) != PAPI_OK) {
    	log_papi_critical(ret_val, "PMC events start error");
        return -1;
    }

    return 0;
}

void pmc_events_stop_local_thread() {
	long long values[MAX_NUM_EVENTS];

	assert(tls_event_set != PAPI_NULL);

    PAPI_stop(tls_event_set, values);
}

int pmc_events_read_local_thread(long long *values) {
    int ret_val;
//    int status = 0;

    assert(values);

//    PAPI_state(event_set, &status);
//    if (status != PAPI_RUNNING) {
//        DBG_LOG(CRITICAL, "PMC event set not in running state");
//        return -1;
//    }

    if ((ret_val = PAPI_read(tls_event_set, values)) != PAPI_OK) {
    	log_papi_critical(ret_val, "PMC events read error");
        return -1;
    }

    if ((ret_val = PAPI_reset(tls_event_set)) != PAPI_OK) {
        log_papi_critical(ret_val, "PMC events reset error");
        return -1;
    }

    return 0;
}
