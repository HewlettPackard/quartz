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

#awk '($1~/physical_nodes/) {print;}'  nvmemul.ini

echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

dir_name_res=ORIG-lat-test

rm -f foo*
rm -rf $dir_name_res
mkdir $dir_name_res


cp  nvmemul-debug.ini  nvmemul.ini
../build/bench/memlat/memlat 1 1 1 1000000 64 8 0 0

for time in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
do
    ../build/bench/memlat/memlat 1 1 1 1000000 64 8 0 0 > $dir_name_res/foo-hw-latency.txt
    grep "measuring latency: latency is" $dir_name_res/foo-hw-latency.txt > $dir_name_res/foo
    awk 'NR==1 {local=$7;}
         END {print local}'  $dir_name_res/foo >>  $dir_name_res/list-hw-latency.txt
done

echo "#FORMAT:#1_min #2_aver #3_max" > $dir_name_res/final-hw-latency.txt  

awk 'BEGIN {max1 = 0.0; min1 = 10000000.0; sum1 = 0.0;}
         ($1 > max1){max1 = $1;}
         ($1 < min1){min1 = $1;}
         {sum1=sum1+$1;sum2=sum2+$2;}
         END {print min1, sum1/NR, max1;}'  $dir_name_res/list-hw-latency.txt  >> $dir_name_res/final-hw-latency.txt  

rm  $dir_name_res/foo*



















