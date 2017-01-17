#!/bin/bash
#################################################################
#Copyright 2016 Hewlett Packard Enterprise Development LP.  
#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation; either version 2 of the License, or (at
#your option) any later version. This program is distributed in the
#hope that it will be useful, but WITHOUT ANY WARRANTY; without even
#the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
#PURPOSE. See the GNU General Public License for more details. You
#should have received a copy of the GNU General Public License along
#with this program; if not, write to the Free Software Foundation,
#Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#################################################################

NVM_EMUL_PATH="`dirname $0`/.."

device_name="nvmemul"
device_module_name=${device_name}".ko"
device_path="/dev/${device_name}"
device_module_path=`find ${NVM_EMUL_PATH}/build -name ${device_module_name}`


function loaddev {
    if [ -z "${device_module_path}" ]; then
        echo "Module not found. Compile the emulator's source code first."
        exit -1
    fi

    /sbin/insmod ${device_module_path} 2> /dev/null

    if [ $? -ne 0 ]; then
        lsmod | grep ${device_name} > /dev/null
        if [ $? -eq 0 ]; then
            echo "Kernel module already loaded, please reload it."
            exit 1
        fi
        echo "Kernel module loading failed"
        exit 1
    fi

    device_major=`grep ${device_name} /proc/devices | awk '{ print $1 }'`
    if [ $? -ne 0 -o -z "${device_major}" ]; then
        echo "Failed to detect module major"
        exit 1
    fi

    rm -f ${device_path}
    if [ $? -ne 0 ]; then
        echo "Failed to delete kernel module device file"
        exit 1
    fi

    mknod ${device_path} c ${device_major} 0
    chmod a+wr ${device_path}

    lsmod | grep ${device_name} > /dev/null

    if [ $? -eq 0 ]; then
        echo "Kernel module loaded successfully"
    else
        echo "kernel module loading failed"
        exit 1
    fi
}

function unloaddev {
    /sbin/rmmod ${device_name} 2> /dev/null
    rm -f ${device_path}
    if [ $? -eq 0 ]; then
        echo "Kernel module unloaded successfully"
    else
        echo "Failed to delete kernel module device file"
        exit 1
    fi
}

function help() {
    echo "$0 <load|unload|reload>"
}

### MAIN ###

if [ $(id -u) -ne 0 ]; then
   echo "You mut be root to execute this script"
   exit -1
fi

if [ $# -eq 0 ]; then
    help
    exit 1
fi

if [ "$1" = "load" ] || [ "$1" = "l" ]; then
    loaddev
elif [ "$1" = "unload" ] || [ "$1" = "u" ]; then
    unloaddev
elif [ "$1" = "reload" ] || [ "$1" = "r" ]; then
    unloaddev
    loaddev
else
    help
    exit 1
fi

exit 0
