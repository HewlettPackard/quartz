**For testing whether your environment is configured correctly for
running Quartz** (e.g., whether you set all the required environmental
variables, etc.) **we have created a few scripts with benchmarks, which
can be executed automatically** and which can provide you with a
feedback on Quartz performance in your environment.

**The directory with these scripts is called: *benchmark-tests*. There are three scripts which you can run:**
- **bandwidth-model-building.sh**

   This script will execute for approximately **10 min** and will build a memory
   bandwidth model that can be used in the experiments with memory bandwidth
   throttling. The configuration file uses a "debug" mode on purpose -- that
   you can see the messages on the screen about the progress of the memory
   bandwidth  model building, which can be found at */tmp/bandwidth_model*

- **memlat-orig-lat-test.sh**

   This script will measure your server hardware memory access latency: local
   and remote (for two sockets servers).  It will execute the test 20 times, and   write the results in directory *ORIG-lat-test*
   You can find the summary of the results in the file *ORIG-lat-test/final-hw-latency.txt*.
   It will have measurements like:

     > *FORMAT:  1_min_local  2_aver_local  3_max_local  4_min_remote  5_aver_remote  6_max_remote*

     > *91      91.9      92     152    163.9     176*

    First three numbers show: minimal, average and maximum measured local
    memory access latency (out of 20 measurements). The last three numbers
    show show similar measurements for  access latency of the remote memory,
    i.e., in the second socket.

-  **memlat-bench-test-10M.sh**

    This script will execute memlat benchmark (pointer-chasing benchmark) with
    nine emulated memory access latencies: 200 ns, 300 ns,..., 1000 ns.
    It will run the benchmark with these emulated latencies in two settings:
    in the local socket (.i.e., emulating a higher memory access latency in the
    local socket) and similarly, in the remote socket.
    Each test is repeated 10 times: this is used for assessing the variability
    of  your environment. In some cases, we had issues with TurboBoost mode, \
    which did impact the quality of the emulation...
    This test might take **approx. 30 min to finish** (since it executes 180 tests),
    and will create two output directories:  *FULL-RESULTS-test*  and
    *SUMMARY-RESULTS-test*
    In the directory SUMMARY-RESULTS-test, you will find two files that
    summarize the outcome of the experiments in the local and remote sockets.
    The outcome should look like this:

    >*FORMAT: 1_emul_lat(ns) 2_min_meas_lat(ns)  3_aver_meas_lat(ns)  4_max_meas_lat(ns)  5_aver_error(%) 6_max_error(%)*

    >*200 177 197.9 204 1.05 11.5*

    >*300 259 289.5 300 3.5 13.6667*

    >*400 354 382.6 395 4.35 11.5*

    >*500 468 485.8 490 2.84 6.4*

    >*600 554 575.3 585 4.11667 7.66667*

    >*700 640 666.6 681 4.77143 8.57143*

    >*800 749 766.4 776 4.2 6.375*

   >*900 851 866.2 871 3.75556 5.44444*

    >*1000 926 956.5 966 4.35 7.4*


    *The format is the following:*

    *1st column:* emulated latency

    *2nd column:* minimum measured  latency (across 10 tests)

    *3d column:* average measured  latency (across 10 tests)

    *4th column:* maximum measured  latency (across 10 tests)

    *5th column:* average error (between emulated and measured latencies)

    *6th column:* max error (between emulated and measured latencies)

One of the goals of the designed performance emulator is to provide a
framework for application sensitivity studies under different
latencies and memory bw. Even if you have 15% deviation (error) from
the targeted emulated latencies, but the benchmark measurements are
consistent -- this is a good sign that you can perform a good
sensitivity study.
