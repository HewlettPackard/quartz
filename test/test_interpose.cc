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
#include <stdio.h>
#include "gtest/gtest.h"

static int interpose_pthread_create_success = 0;


// Ugly hack: we want to test whether interposition works. To do this we 
// hook on the functions that the interposition code calls by redefining these
// functions. As those functions are written in C, we need to make sure we force
// the C++ compiler use C linkage.

#ifdef __cplusplus
extern "C" {
#endif

// this function is called when interposition of pthread_create is successful
int register_thread(pthread_t thread)
{
    interpose_pthread_create_success = 1;
    return 0;
}

#ifdef __cplusplus
}
#endif

void* interpose_pthread_create_start_routine(void* args)
{
    return NULL;
}

void interpose_pthread_create()
{
    pthread_t thread;  
    
    pthread_create (&thread, NULL, &interpose_pthread_create_start_routine, NULL);

    pthread_join(thread, NULL);
              
}

void interpose_pthread_mutex_lock(pthread_mutex_t* lock)
{
    pthread_mutex_lock(lock);
}

void interpose_pthread_mutex_unlock(pthread_mutex_t* lock)
{
    pthread_mutex_unlock(lock);
}

TEST(Interpose, pthread_create)
{
    EXPECT_EQ(0, interpose_pthread_create_success);
    interpose_pthread_create();
    EXPECT_EQ(1, interpose_pthread_create_success);
}

TEST(Interpose, pthread_mutex_lock)
{
    //EXPECT_EQ(1, 0);
}


int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();

    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);
    interpose_pthread_mutex_lock(&lock);
    interpose_pthread_mutex_unlock(&lock);
}
