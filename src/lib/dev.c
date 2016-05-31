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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include "dev/ioctl_query.h"
#include "error.h"
#include "dev.h"

// TODO: get this value from the config file
#define DEV_PATH "/dev/nvmemul"

int set_counter(unsigned int counter_id, unsigned int event_id)
{
    int fd;
    int ret;

    ioctl_query_setcounter_t q;
    fd = open(DEV_PATH, O_RDONLY);
    if (fd < 0) {
        DBG_LOG(ERROR, "Can't open %s - Is the NVM emulator device driver installed?\n", DEV_PATH);
        return E_ERROR;
    }
    q.counter_id = counter_id;
    q.event_id = event_id;
    if ((ret = ioctl(fd, IOCTL_SETCOUNTER, &q)) < 0) {
    close(fd);
        return E_ERROR;
    }
    close(fd);
    return E_SUCCESS;
}


int set_pci(unsigned int bus_id, unsigned int device_id, unsigned int function_id, unsigned int offset, uint16_t val)
{
	int fd; 
    int ret;

    ioctl_query_setgetpci_t q;
	fd = open(DEV_PATH, O_RDONLY);
	if (fd < 0) {
		DBG_LOG(ERROR, "Can't open %s - Is the NVM emulator device driver installed?\n", DEV_PATH);
		return E_ERROR;
	}
    q.bus_id = bus_id;
    q.device_id = device_id;
    q.function_id = function_id;
    q.offset = offset;
    q.val = val;
    if ((ret = ioctl(fd, IOCTL_SETPCI, &q)) < 0) {
    	close(fd);
        return E_ERROR;
    }
	close(fd);
    return E_SUCCESS;
}

int get_pci(unsigned int bus_id, unsigned int device_id, unsigned int function_id, unsigned int offset, uint16_t* val)
{
	int fd; 
    int ret;

    ioctl_query_setgetpci_t q;
	fd = open(DEV_PATH, O_RDWR);
	if (fd < 0) {
		DBG_LOG(ERROR, "Can't open %s - Is the NVM emulator device driver installed?\n", DEV_PATH);
		return E_ERROR;
	}
    q.bus_id = bus_id;
    q.device_id = device_id;
    q.function_id = function_id;
    q.offset = offset;
    q.val = 0;
    if ((ret = ioctl(fd, IOCTL_GETPCI, &q)) < 0) {
    	close(fd);
        return E_ERROR;
    }
    *val = q.val;
	close(fd);
    return E_SUCCESS;
}


