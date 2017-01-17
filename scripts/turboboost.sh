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

function usage()
{
    echo "$0 <function> [target CPU id]"
    echo -e "\tfunctions:"
    echo -e "\t\t check: verifies if a given CPU id has Turbo Boost enabled"
    echo -e "\t\t disable: disables a given CPU id or all CPUs if not specified"
    echo -e "\t\t enabled: enables a given CPU id or all CPUs if not specified"
}

function verify_cpu_id()
{
    re='^[0-9]+$'
    if ! [[ $1 =~ $re ]]; then
        echo "CPU id is not a number"
        exit 1
    fi
}

function check_msr_module()
{
    lsmod | grep msr > /dev/null
    if [ $? -ne 0 ]; then
         # some systems need this, others don't
        sudo modprobe msr &> /dev/null
        #if [ $? -ne 0 ]; then
        #    echo "Failed to load MSR module"
        #    exit 1
        #fi
    fi
}

function check()
{
    cpu=$1

    if [ -z "${cpu}" ]; then
        usage
        exit 1
    fi

    cpus=$(lscpu | sed -n 4p | awk '{ print $2 }')

    if [ ${cpu} -ge ${cpus} ]; then
        echo "CPU id out of range"
        exit 1
    fi

    disabled=$(sudo rdmsr -p${cpu} 0x1a0 -f 38:38)

    if [ "${disabled}" == "1" ]; then
        echo "Turbo Boost for processor ${cpu} is disabled"
    else
        echo "Turbo Boost for processor ${cpu} is enabled"
    fi
}

function enable()
{
    cpu=$1

    cpus=$(lscpu | sed -n 4p | awk '{ print $2 }')

    if [ -z "${cpu}" ]; then
        for (( i=0; i<${cpus}; i++ )); do 
            sudo wrmsr -p$i 0x1a0 0x850089
        done
        echo "Turbo Boost enabled for all CPUs"
    else
        if [ ${cpu} -ge ${cpus} ]; then
            echo "CPU id out of range"
            exit 1
        fi
        sudo wrmsr -p${cpu} 0x1a0 0x850089
        echo "Turbo Boost enabled for CPU ${cpu}"
    fi
}

function disable()
{
    cpu=$1

    cpus=$(lscpu | sed -n 4p | awk '{ print $2 }')

    if [ -z "${cpu}" ]; then
        for (( i=0; i<${cpus}; i++ )); do 
            sudo wrmsr -p$i 0x1a0 0x4000850089;
        done
        echo "Turbo Boost disabled for all CPUs"
    else
        if [ ${cpu} -ge ${cpus} ]; then
            echo "CPU id out of range"
            exit 1
        fi
        sudo wrmsr -p${cpu} 0x1a0 0x4000850089;
        echo "Turbo Boost disabled for CPU ${cpu}"
    fi
}



### MAIN ###

if [ $# -eq 0 ]; then
    usage
    exit 1
fi

funct=$1
target_cpu=$2

check_msr_module

if [ ! -z "${target_cpu}" ]; then
    verify_cpu_id ${target_cpu}
fi

case ${funct} in
    "enable")
        enable ${target_cpu}
        ;;
    "disable")
        disable ${target_cpu}
        ;;
    "check")
        check ${target_cpu}
        ;;
    *)
        usage
        exit 1
esac

exit 0

