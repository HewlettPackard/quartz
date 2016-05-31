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
#ifndef __DEVICE_DRIVER_API_H
#define __DEVICE_DRIVER_API_H

#include <stdint.h>

#define MAX_NUM_MC_PCI_BUS 16
#define MAX_NUM_MC_CHANNELS 16

typedef struct {
    unsigned int bus_id;
    unsigned int dev_id;
    unsigned int funct;
} pci_addr;

typedef struct {
    pci_addr addr[MAX_NUM_MC_CHANNELS];
    unsigned int channels;
} pci_regs_t;

int set_counter(unsigned int counter_id, unsigned int event_id);
int set_pci(unsigned bus_id, unsigned int device_id, unsigned int function_id, unsigned int offset, uint16_t val);
int get_pci(unsigned bus_id, unsigned int device_id, unsigned int function_id, unsigned int offset, uint16_t* val);

#endif /* __DEVICE_DRIVER_API_H */
