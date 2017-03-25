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
#ifndef __PFLUSH_H
#define __PFLUSH_H

/**
 * \file
 * 
 * \page pflush_api Persistent Memory API 
 *
 * Method to be used by client to inject a write latency.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void init_pflush(int cpu_speed_mhz, int write_latency_ns);

/**
 * \brief Flush the cacheline containing address addr.
 */
void pflush(uint64_t *addr);

#ifdef __cplusplus
}
#endif

#endif /* __PFLUSH_H */
