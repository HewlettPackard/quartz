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
/*
 * process_rank.c
 *
 *  Created on: Jun 16, 2015
 *      Author: root
 */


#include <unistd.h>
#include "model.h"
#include "error.h"

#define EMUL_LOCAL_PROCESSES_VAR "EMUL_LOCAL_PROCESSES"

#define EMUL_LOCK_FILE "/tmp/emul_lock_file"
#define EMUL_PROCESS_LOCAL_RANK_FILE "/tmp/emul_process_local_rank"
#define LOCKED_WAIT_US 1000
#define MAX_LOCKED_RETRIES 50

extern latency_model_t latency_model;

int set_process_local_rank()
{
    FILE *flock = NULL;
    FILE *fcounter = NULL;
    int expired = 0;
    int process_id = 0;
    char *processes;
    int ret = E_SUCCESS;
#ifndef NDEBUG
    char hname[64];
#endif

    processes = getenv(EMUL_LOCAL_PROCESSES_VAR);

    if (!processes) {
    	DBG_LOG(WARNING, "No %s variable set, skipping rank setting\n", EMUL_LOCAL_PROCESSES_VAR);
    	return E_SUCCESS;
    } else {
    	if (sscanf(processes, "%d", &latency_model.max_local_processe_ranks) != 1) {
    		DBG_LOG(WARNING, "Ignoring EMUL_PROCESSES_PER_SYSTEM variable with invalid value '%s'\n", processes);
    		return E_SUCCESS;
    	}
    }

    if (latency_model.max_local_processe_ranks < 2) {
    	DBG_LOG(WARNING, "EMUL_PROCESSES_PER_SYSTEM value is %d, skipping rank setting\n",
    			latency_model.max_local_processe_ranks);
    	return E_SUCCESS;
    }

    DBG_LOG(DEBUG, "setting process local rank for %d local processes\n",
    		latency_model.max_local_processe_ranks);

    while (expired < MAX_LOCKED_RETRIES) {
    	// open lock file on exclusive mode
        flock = fopen(EMUL_LOCK_FILE, "wx");

        if (flock == NULL) {
//        	DBG_LOG(DEBUG, "failed to create lock file\n");
            usleep(LOCKED_WAIT_US);
            expired++;
        }
        if (flock) break;
    }
    if (expired >= MAX_LOCKED_RETRIES) {
    	DBG_LOG(ERROR, "failed to set process local rank\n");
    	return E_ERROR;
    }

    // lock acquired, read process counter file
    if (access(EMUL_PROCESS_LOCAL_RANK_FILE, R_OK | W_OK) < 0) {
    	// rank file does not exist, create it and write "1" for next process
    	// this process rank id is 1
    	process_id = 1;
    	fcounter = fopen(EMUL_PROCESS_LOCAL_RANK_FILE, "w");
    	fwrite(&process_id, sizeof(int), 1, fcounter);
    	fclose(fcounter);
    } else {
    	// rank file exists, read the current rank max value and use it as this process local
    	// rank id and increment the value in the rank file for the next process
    	fcounter = fopen(EMUL_PROCESS_LOCAL_RANK_FILE, "r+");
    	if (fread(&process_id, sizeof(int), 1, fcounter) == 0) {
    	    abort();
    	}
    	DBG_LOG(DEBUG, "read from file current max rank %d\n", process_id);
    	latency_model.process_local_rank = process_id;
    	process_id++;
    	if (process_id >= latency_model.max_local_processe_ranks) {
    	    DBG_LOG(ERROR, "process rank %d exceeded limit of %d max emulated processes\n",
    	        process_id, latency_model.max_local_processe_ranks);
    	    fclose(fcounter);
    	    ret = E_ERROR;
    	} else {
    	    DBG_LOG(DEBUG, "write to file new max rank %d\n", process_id);
    	    rewind(fcounter);
            fwrite(&process_id, sizeof(int), 1, fcounter);
            fclose(fcounter);
        }
    }

    // close and delete lock file
    fclose(flock);
    remove(EMUL_LOCK_FILE);

#ifndef NDEBUG
    gethostname(hname, sizeof(hname));
    DBG_LOG(DEBUG, "process local rank is %d on system %s\n", latency_model.process_local_rank, hname);
#endif

    return ret;
}

int unset_process_local_rank()
{
    FILE *flock = NULL;
    FILE *fcounter = NULL;
    int expired = 0;
    int process_id;

    if (latency_model.max_local_processe_ranks < 2) {
    	return E_SUCCESS;
    }

    DBG_LOG(DEBUG, "Unsetting process local rank\n");

    while (expired < MAX_LOCKED_RETRIES) {
    	// open lock file on Exclusive mode
        flock = fopen(EMUL_LOCK_FILE, "wx");

        if (flock == NULL) {
//        	DBG_LOG(DEBUG, "failed to create lock file\n");
            usleep(LOCKED_WAIT_US);
            expired++;
        }
        if (flock) break;
    }
    if (expired >= MAX_LOCKED_RETRIES) {
    	DBG_LOG(ERROR, "failed to unset process local rank\n");
    	return E_ERROR;
    }

    // lock acquired, read process counter file
    if (access(EMUL_PROCESS_LOCAL_RANK_FILE, R_OK | W_OK) == 0) {
    	// if rank file does not exist, nothing to be done
    	// file exists, read the current value and decrement it
    	fcounter = fopen(EMUL_PROCESS_LOCAL_RANK_FILE, "r+");
    	if (fread(&process_id, sizeof(int), 1, fcounter) == 0) {
    	    abort();
    	}
    	DBG_LOG(DEBUG, "Exiting process and reading current rank max %d\n", process_id);
    	if (process_id > 0) process_id--;
    	{
    	char hname[64];
    	gethostname(hname, sizeof(hname));
    	DBG_LOG(DEBUG, "Exiting process and writing new rank max %d on %s\n", process_id, hname);
    	}
    	rewind(fcounter);
		fwrite(&process_id, sizeof(int), 1, fcounter);
		fclose(fcounter);
    }

    // close and delete lock file
    fclose(flock);
    remove(EMUL_LOCK_FILE);

    return E_SUCCESS;
}
