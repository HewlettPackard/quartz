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

#include "cpu.h"

// later, cpu_model_name() is used to distinguish between
// Xeon and non-Xeon processors. It's much easier here
// to consider all processors non-Xeon.
// references:
// 1- http://a4lg.com/tech/x86/database/x86-families-and-models.en.html
// 2- Intel® Xeon® Processor E7-8800/4800 v3 Product Family Specification
// 3- https://software.intel.com/en-us/articles/intel-architecture-and-processor-identification-with-cpuid-model-and-family-numbers
microarch_ID_t known_cpus[] =
    {
        // order does not matter
        {.family = 0x06, .model = 0x2A, .microarch = SandyBridge},
        {.family = 0x06, .model = 0x2D, .microarch = SandyBridge},

        {.family = 0x06, .model = 0x3A, .microarch = IvyBridge},
        {.family = 0x06, .model = 0x3E, .microarch = IvyBridge},

        {.family = 0x06, .model = 0x3C, .microarch = Haswell},
        {.family = 0x06, .model = 0x3F, .microarch = Haswell},
        {.family = 0x06, .model = 0x45, .microarch = Haswell},
        {.family = 0x06, .model = 0x46, .microarch = Haswell},

        // must be the last element
        {.family = 0x0, .model = 0x0, .microarch = Invalid}};

// order must correspond to microarch_t
char *microarch_strings[] =
    {
        "Invalid",
        "Sandy Bridge",
        "Sandy Bridge Xeon",
        "Ivy Bridge",
        "Ivy Bridge Xeon",
        "Haswell",
        "Haswell Xeon"};

#endif /* __KNOWN_CPUS_H */
