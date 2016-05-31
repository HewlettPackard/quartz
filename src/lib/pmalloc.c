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
#include <numa.h>
#include "topology.h"
#include "pmalloc.h"
#include "thread.h"
#include "debug.h"

// pmalloc should be implemented as a separate library

// FIXME: pmalloc currently uses numa_alloc_onnode() which is slower than regular malloc.
// Consider layering another malloc on top of a emulated nvram 


void* pmalloc(size_t size)
{
    thread_t* thread = thread_self();

    if (thread == NULL) {
    	// FIXME: JVM for instance create threads using a mechanism not traced by this emulator
    	//        for now we make sure the current thread is registered right when it makes the
    	//        first explicit NVM allocation. A better solution is to trace the thread creation
    	//        done by JVM.
        register_self();
        thread = thread_self();
    }

    if (thread) {
        return numa_alloc_onnode(size, thread->virtual_node->nvram_node->node_id);
    } else {
    	DBG_LOG(ERROR, "pmalloc called with NULL thread\n");
    }
    
    return NULL;
}

void *prealloc(void *old_addr, size_t old_size, size_t new_size)
{
    return numa_realloc(old_addr, old_size, new_size);
}

void pfree(void* start, size_t size)
{
    numa_free(start, size);
}
