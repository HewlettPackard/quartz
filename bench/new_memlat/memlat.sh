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
#!/bin/bash

# percentage of error as threshold to discard outliers, anything above this percentage will be discarded
MAX_ERROR_PERCENTAGE=10
# max number of tries to execute memlat
MAX_TRIES=10


TEMP_FILE=/tmp/tmp_memlat.out


NVM_EMUL_PATH="`dirname $0`/../.."
NELEMS=$1
TARGET_DRAM=$2


function usage()
{
    echo "$0 [number of elements] [0=local dram|1=remote dram]"
    exit 1
}

function validate_decimal()
{
    re='^[0-9]+$'
    if ! [[ $1 =~ $re ]] ; then
        return 1
    fi
    return 0
}

function check_parameters()
{
    if [ $# -ne 2 ]; then
        echo "Incorrect arguments"
        usage
    fi

    validate_decimal ${NELEMS}

    if [ $? -ne 0 ]; then
        echo "Invalid number of arguments"
        usage
    fi

    if [ ${TARGET_DRAM} -ne 0 -a ${TARGET_DRAM} -ne 1 ]; then
        echo "Incorret dram target"
        usage
    fi
}

function verify_run
{
    target=$(cat ${TEMP_FILE} | grep "target latency" | awk '{ print $3 }')
    measured=$(cat ${TEMP_FILE} | grep "measured latency" | awk '{ print $4 }')

    if [ ${measured} -gt ${target} ]; then
        delta=$(expr ${measured} - ${target});
    else
        delta=$(expr ${target} - ${measured});
    fi

    if [ ${target} -gt 0 ]; then
        error=$(expr ${delta} \* 100)
        error=$(expr ${error} \/ ${target})
    else
        error=0
    fi


    if [ ${error} -gt ${MAX_ERROR_PERCENTAGE} ]; then
        return 1
    fi

    return 0
}

############ MAIN ######################

check_parameters $*

# execute memlat in loop until the result is within the threshold or the max tries is reached
for (( c=0; c<${MAX_TRIES}; c++ )); do
    ${NVM_EMUL_PATH}/scripts/runenv.sh ${NVM_EMUL_PATH}/build/bench/new_memlat/new_memlat 1 1 1 ${NELEMS} 64 8 0 ${TARGET_DRAM} &> ${TEMP_FILE}

    verify_run

    ret=$?

    if [ ${ret} -eq 0 ]; then
        cat ${TEMP_FILE} | grep "measured latency"
        break
    fi
done

if [ ${ret} -ne 0 ]; then
    echo "Could not produce a valid run"
fi

rm -f ${TEMP_FILE}

exit ${ret}
