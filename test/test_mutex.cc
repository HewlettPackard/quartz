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
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include "gtest/gtest.h"

#define MAX_NUM_THREADS 128

pthread_mutex_t mutex;

void* worker(void* args) 
{
//    int i;
//    char* array = (char*) malloc(1024*1024);

    pthread_mutex_lock(&mutex);

    pthread_mutex_unlock(&mutex);
    return NULL;
}


int main(int argc, char** argv)
{
	pthread_t thread[MAX_NUM_THREADS];
	int thread_count = 4;
	int i;
//    int sum;

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&mutex);
	for (i = 0; i< thread_count; i++)	
		pthread_create(&thread[i], NULL, worker, NULL);

	for(i = 0 ; i < thread_count ; i++)
		pthread_join(thread[i], NULL);
}
