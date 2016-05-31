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
#ifndef __PMALLOC_H
#define __PMALLOC_H

/**
 * \file
 * 
 * \page pmalloc_api Persistent Memory API 
 *
 * Methods to be used by clients to allocate and free emulated NVRAM.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *pmalloc(size_t size);
void *prealloc(void *old_addr, size_t old_size, size_t new_size);
void pfree(void *start, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __PMALLOC_H */
