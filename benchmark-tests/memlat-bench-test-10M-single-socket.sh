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

dir_name_res=FULL-RESULTS-test
dir_name_sum=SUMMARY-RESULTS-test

rm -rf $dir_name_sum
mkdir  $dir_name_sum

rm -f foo*
rm -rf $dir_name_res
mkdir $dir_name_res

cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >> $dir_name_res/foo-runs-test

cp nvmemul-orig.ini nvmemul.ini
../build/bench/memlat/memlat 1 1 1 1000000 64 8 0 0 >foo


    for numchains in 1 
    do
	for epoch in 10000 
	do 
	    echo "#FORMAT #1_emul_lat(ns) #2_min_meas_lat(ns)  #3_aver_meas_lat(ns)  #4_max_meas_lat(ns)  #5_aver_error(%) #6_max_error(%)" >  $dir_name_sum/summary-nvm-lat-accuracy-epoch-$epoch-numchains-$numchains.txt

	    for lat in 200 300 400 500 600 700 800 900 1000
	    do
		awk 'BEGIN {read_lat = substr(ARGV[2],3); epoch_lat = substr(ARGV[3],3);}
(!(NR==7 || NR==9 || NR==10 || $1~/physical_nodes/)){ print;}
(NR==7){ print $1,$2, read_lat,";";}
(NR==9){ print $1,$2, epoch_lat,";";}
(NR==10){ print $1,$2, epoch_lat,";";}
($1~/physical_nodes/) {print $1,$2,"\"0\""";";}
' nvmemul-orig.ini v=$lat v=$epoch > foo-nvmemul-$lat-$epoch.ini
		mv foo-nvmemul-$lat-$epoch.ini  nvmemul.ini
		echo "lat epoch chains" $lat $epoch $numchains >>   $dir_name_res/foo-runs
		
		for time in 1 2 3 4 5 6 7 8 9 10
		do
		    ../build/bench/memlat/memlat 1 1 $numchains 10000000 64 8 0 0 >> $dir_name_res/full_results-$lat-$epoch-$numchains.txt
 		done
                grep latency_ns $dir_name_res/full_results-$lat-$epoch-$numchains.txt > $dir_name_res/results-$lat-$epoch-$numchains.txt
		awk 'BEGIN {max = 0; min = 1000000; sum = 0; aver=0.0; max_error=0.0; aver_error=0.0;read_lat = substr(ARGV[2],3);epoch_lat = substr(ARGV[3],3); MPL = substr(ARGV[4],3); }
($2 > max){max = $2;}
($2 < min){min = $2;}
{sum=sum+$2; if ($2 < read_lat*1.0) {error=read_lat -$2} else {error=$2 - read_lat}; if (error > max_error) max_error=error;}
END {aver=sum/NR; if (aver < read_lat*1.0) {aver_error = (read_lat - aver)*100.0/read_lat} else {aver_error = (aver - read_lat )*100.0/read_lat}; print read_lat, min,aver,max, aver_error,max_error*100.0/read_lat;} '   $dir_name_res/results-$lat-$epoch-$numchains.txt v=$lat v=$epoch v=$numchains >> $dir_name_sum/summary-nvm-lat-accuracy-epoch-$epoch-numchains-$numchains.txt
		
	    done
	done
    done


#FORMAT_summary-results: #1_nvm_lat(ns) #2_min_nvm_lat(ns)  #3_aver_nvm_lat(ns)  #4_max_nvm_lat(ns)  #5_aver_error(%) #6_max_error(%)

#parameter is nvm_lat



