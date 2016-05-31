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
#ifndef __KNOWN_CPUS_H
#define __KNOWN_CPUS_H

#include "xeon-ex.h"

// XEON E5 (SandyBridge) and E5/E7 v2 (IvyBridge) may have the same CPU numbers, except by the "v2" on IvyBridge (and v3 on Haswell).
// Make sure the v2, v3, .. are defined before SandyBridge entry below as a workaround for the CPU match algorithm.
cpu_model_t* known_cpus[] = {
	&cpu_model_intel_xeon_ex_v3,
    &cpu_model_intel_xeon_ex_v2,
    &cpu_model_intel_xeon_ex,
    0
};


#endif /* __KNOWN_CPUS_H */
