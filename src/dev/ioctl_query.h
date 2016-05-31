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
#ifndef __IOCTL_QUERY_H
#define __IOCTL_QUERY_H

#include <linux/ioctl.h>

#define MYDEV_MAGIC (0xAA)

typedef struct { 
    unsigned int counter_id;
    unsigned int event_id;
} ioctl_query_setcounter_t;

typedef struct { 
    unsigned int bus_id;
    unsigned int device_id;
    unsigned int function_id;
    unsigned int offset;
    unsigned int val;
} ioctl_query_setgetpci_t;

#define IOCTL_SETCOUNTER _IOR(MYDEV_MAGIC, 0, ioctl_query_setcounter_t *) 
#define IOCTL_SETPCI     _IOR(MYDEV_MAGIC, 1, ioctl_query_setgetpci_t *) 
#define IOCTL_GETPCI     _IOWR(MYDEV_MAGIC, 2, ioctl_query_setgetpci_t *) 


#endif /* __IOCTL_QUERY_H */
