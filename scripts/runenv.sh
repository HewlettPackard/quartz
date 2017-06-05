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


if [ -z "$1" ]; then
    echo "runenv.sh [cmd to run]"
    exit 1
fi

rootdir="$NVM_EMUL_PATH"
bindir=$rootdir"/build"

if [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
    current_scaling=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor);

    if [ "${current_scaling}" != "performance" ]; then
        file_list=$(ls /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor)
        for cpu_file in ${file_list}; do
            echo "performance" | sudo tee ${cpu_file} > /dev/null
        done
    fi
fi

$rootdir/scripts/turboboost.sh disable

v=$(uname -r | cut -d '.' -f1)
if [ $v -ge 4 ]; then
    echo "2" | sudo tee /sys/bus/event_source/devices/cpu/rdpmc
fi

export LD_PRELOAD=$bindir"/src/lib/libnvmemul.so"
export NVMEMUL_INI=$rootdir"/nvmemul.ini"

if [ ! -f ${LD_PRELOAD} ]; then
    echo "Library not found. Compile the emulator's library first."
    exit -1
fi

echo $LD_PRELOAD
echo $NVMEMUL_INI

# execute the command passed as argument
$@

