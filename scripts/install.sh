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

PAPI_MAJOR=5
PAPI_MINOR=1
PAPI_RELEASE=1

CMAKE_MAJOR=2
CMAKE_MINOR=8

function install_deps_rpm() {
    yum install -q -y numactl-devel libconfig libconfig-devel cmake kernel-devel-`uname -r` msr-tools uthash-devel

    if [ $? -ne 0 ]; then
        echo "Dependencies installation failed"
        exit -1
    fi
}

function install_deps_deb() {
    apt-get install -y libnuma-dev libconfig-dev cmake  msr-tools uthash-dev

    if [ $? -ne 0 ]; then
        echo "Dependencies installation failed"
        exit -1
    fi
}

function check_supported_papi() {
    major=`papi_version | cut -d ' ' -f3 | cut -d '-' -f1 | cut -d '.' -f1`
    minor=`papi_version | cut -d ' ' -f3 | cut -d '-' -f1 | cut -d '.' -f2`
    release=`papi_version | cut -d ' ' -f3 | cut -d '-' -f1 | cut -d '.' -f3`

    if [ ${major} -ne ${PAPI_MAJOR} ]; then
        echo "CMake version (${major}.${minor}.${release}) not supported (=${PAPI_MAJOR}.${PAPI_MINOR}.${PAPI_RELEASE})"
        exit -1
    fi
    if [ ${minor} -ne ${PAPI_MINOR} ]; then
        echo "CMake version (${major}.${minor}.${release}) not supported (=${PAPI_MAJOR}.${PAPI_MINOR}.${PAPI_RELEASE})"
        exit -1
    fi
    if [ ${release} -ne ${PAPI_RELEASE} ]; then
        echo "CMake version (${major}.${minor}.${release}) not supported (=${PAPI_MAJOR}.${PAPI_MINOR}.${PAPI_RELEASE})"
        exit -1
    fi
}

function check_supported_cmake() {
    major=`cmake -version | head -1 | cut -d ' ' -f3 | cut -d '-' -f1 | cut -d '.' -f1`
    minor=`cmake -version | head -1 | cut -d ' ' -f3 | cut -d '-' -f1 | cut -d '.' -f2`
    
    if [ ${major} -lt ${CMAKE_MAJOR} ]; then
        echo "CMake version (${major}.${minor}) not supported (>=${CMAKE_MAJOR}.${CMAKE_MINOR})"
        exit -1
    fi
    if [ ${major} -eq ${CMAKE_MAJOR} ]; then
        if [ ${minor} -lt ${CMAKE_MINOR} ]; then
            echo "CMake version (${major}.${minor}) not supported (>=${CMAKE_MAJOR}.${CMAKE_MINOR})"
            exit -1
        fi
    fi
}

function check_supported_versions() {
    check_supported_cmake
#    check_supported_papi
}


#################### MAIN ####################

if [ $(id -u) -ne 0 ]; then
   echo "You mut be root to execute this script"
   exit -1
fi

if [ -f /etc/redhat-release ]; then
    install_deps_rpm
elif [ -f /etc/centos-release ]; then
    install_deps_rpm
elif [ -f /etc/debian_version -o -f /etc/debian-release ]; then
    install_deps_deb
else
    echo "Linux distribution not supported"
    exit -1
fi

check_supported_versions

