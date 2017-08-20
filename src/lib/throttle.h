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
#ifndef __MEMTHROTTLE_H
#define __MEMTHROTTLE_H

#include "cpu/cpu.h"

typedef struct bw_throttle_s {
    unsigned int throttle_reg_val[MAX_THROTTLE_VALUE]; 
    double bw[MAX_THROTTLE_VALUE];
    int npairs;
} bw_throttle_t;

int discover_throttle_values(physical_node_t* phys_node, bw_throttle_t** bw_throttlep);
int bw_throttle_to_xml(xmlTextWriterPtr writer, bw_throttle_t* bw_throttle);
int bw_throttle_from_xml(xmlNode* root, bw_throttle_t** bw_throttlep);

#endif /* __MEMTHROTTLE_H */
