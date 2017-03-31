
Quartz: A DRAM-based performance emulator for NVM
----------------------

Quartz leverages features available in commodity hardware to emulate
different latency and bandwidth characteristics of future
byte-addressable NVM technologies.

Quartz's design, implementation details, evaluation, and overhead  can be found 
in the following research paper:
 - **H. Volos, G. Magalhaes, L. Cherkasova, J. Li: Quartz: A Lightweight 
   Performance Emulator for Persistent Memory Software. In Proc. of the 
   16th ACM/IFIP/USENIX International Middleware Conference, (Middleware'2015),
   Vancouver, Canada, December 8-11, 2015.  and can be downloaded from:
   http://www.jahrhundert.net/papers/middleware2015.pdf**

While the emulator is designed to cover three processor families:
*Sandy Bridge, Ivy Bridge*, and *Haswell* -- we have had the best results
on the *Ivy Bridge* platform. Haswell processor has a TurboBoost feature
that cause higher variance and deviations when emulating higher range
latencies (above 600 ns).

Contributors
----------------------
For a list of contributors see [AUTHORS](https://github.hpe.com/labs/quartz/blob/master/AUTHORS). 

Extended documentation
----------------------
Extended documentation available in Doxygen form. To build and view:

    doxygen
    xdg-open doc/html/index.html


Dependencies
------------
This is the list of libraries and tools used by Quartz:

On RPM based distributions:
- cmake 2.8
- libconfig and libconfig-devel
- numactl-devel
- uthash-devel
- kernel-devel

On Debian based distributions:
- cmake 2.8
- libconfig-dev
- libnuma-dev
- uthash-dev
- linux-headers

You can run 'sudo scripts/install.sh' in order to automatically install these 
dependencies.


Supported environment
---------------------
Currently the latency emulator can be used on Linux with *Sandy Bridge, 
Ivy Bridge*, and *Haswell* Intel processors. For bandwidth emulation support, Intel 
Thermal Memory Controller device is required.
No specific Linux distribution or kernel version is required.


Source code tree overview
-------------------------

    bench             Benchmarks
    doc               Documentation, including Doxygen generated documentation (doc/html)
    src/lib           Emulator main library code
    src/dev           Kernel-module for accessing performance counters and 
                      memory-controller PCI registers
    scripts           Helper scripts to run a program using the emulator and install 
                      dependencies
    test              Several tests and application code examples
    benchmark-tests   Several automated tests with benchmark runs and output analysis 
                      for testing the correctness of configured emulation environment and 
                      the accuracy of expected results

For more details, please see the extended documentation generated using Doxygen.

Building
--------
After installing the dependencies, go to the emulator's source code root folder 
and execute the following steps:

    mkdir build
    cd build
    cmake ..
    make clean all

In order to disable statistics support, replace the third step above with:

    cmake .. -DSTATISTICS=OFF
See more details about statistics on the respective section below.
The emulator library, benchmark and test binaries resulted from the build 
process will be available in the respective subfolder inside the 'build' folder.


Usage
-----
First, load the emulator's kernel module. From the emulator's source code root 
folder, execute:

    sudo scripts/setupdev.sh load

Set your processor to run at maximum frequency to ensure fixed cycle 
rate (as the cycle counter is used to project delay time). You can 
use the scaling governor:

    echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

Set the LD_PRELOAD and NVMEMUL_INI environment variables to point respectively 
to the emulators library and the configuration file to be used. The LD_PRELOAD 
is used for automatically loading the emulator's library when the user 
application is executed. Thus, there is no need to statically link the library 
to the user application. See below details about the configuration file in the 
respective section.

Rather than configuring the scaling governor and the environment variables 
manually as indicated above, you can use the scripts/runenv.sh script. See 
below.

An additional configuration step may be required depending on the Linux Kernel
version. This emulator makes use of rdpmc x86 instruction to read CPU counters.
Before kernel 4.0, when rdpmc support was enabled, any process (not just ones
with an active perf event) could use the rdpmc instruction to access the counters.
Starting with Linux 4.0 rdpmc support is only allowed if an event is currently
enabled in a process's context. To restore the old behavior, write the value 2
to /sys/devices/cpu/rdpmc if kernel version is 4.0 or greater:

    echo 2 | sudo tee /sys/devices/cpu/rdpmc

Run your application:

    scripts/runenv.sh <your_app>

The runenv.sh script runs an application in a new shell environment that
properly sets LD_PRELOAD to the library available in the build folder. We do
not modify the current shell environment to avoid getting other applications 
interposed by the emulator unexpectedly. 

Alternatively, you may directly link 
the library to your application but the nvmemul library must come first in the 
linking order to ensure we properly interpose on necessary functions.
Additionally, this script sets the NVMEMUL_INI environment variable to point
to the nvmemul.ini configuration file available in the emulator's source code 
root folder.


Configuration file
------------------
Emulator runtime parameters can be defined in a configuration file. 

The default path is ./nvmemul.ini but you may change the path through the 
environment variable $NVMEMUL_INI (see scripts/runenv.sh).

The main available parameters are:

    - Latency:
      enable                  True means the latency emulation is on, false,
                              the latency emulation is disabled.
      inject_delay            True means the delay injection is on, false,
                              the emulator will skip the delay injection
      read                    The target read latency in nano seconds. It must 
                              be greater than the hardware latency. This value
                              is automatically consisted by the emulator.
      write                   The target write latency in nano seconds. It must 
                              be greater than the hardware latency. This value
                              is automatically consisted by the emulator.
      max_epoch_duration_us   This is the epoch duration in micro seconds. 
                              Eventually an epoch may be greater than this value
                              depending on signal delivery managed by Kernel.
      min_epoch_duration_us   The minimum epoch duration. 
    - Bandwidth:
      enable                  True means the bandwidth emulation is on, false, 
                              it is disabled.
      model                   File path used by the emulator to cache the 
                              detected hardware bandwidth characteristics.
      read                    Target read bandwidth in MB/s.
      write                   Target write bandwidth in MB/s;
    - Topology:
      mc_pci                  File path used by the emulator to cache the PCI 
                              bus topology. It is not required if bandwidth 
                              emulation is disabled.
      physical_nodes          List all CPU sockets ids to be added to the known
                              topology. An odd number of CPU sockets means it
                              will not be possible to configure all CPUs in
                              pairs and then a single CPU will be used as NVM
                              only. See Emulation modes section below.
    - Statistics:
      enable                  True means the statistics collection and report is
                              enable, false, it is disable. See the Statistics
                              section below.
      file                    File path used by the emulator to write the 
                              statistics report. If not provided, emulator will 
                              use stdout.
    - Debug:
      level                   Shows debugging message with level up to this 
                              value, the greater this value is, the more verbose 
                              the debug log will be.
                              0: off; 1: critical; 2: error; 3: warning; 4: info;
                              5: debugging.
      verbose                 If greater than zero shows source code information
                              along with the debugging message.


Latency emulation modes
-----------------------
The emulator may run application threads on a *NVM only* mode or *DRAM+NVM* mode.
It depends if the system has more than one CPU socket and if the topology 
configuration enables multiple CPU socket.

For *NVM only* mode, the emulator will use a CPU socket with no sibling node and
make use of the DRAM available in that socket to emulate NVM. Any DRAM memory 
access on this socket will produce delays injection to emulate the target 
latency.

For *DRAM+NVM* mode, the emulator will differentiate DRAM from virtual NVM 
latencies. It is supported only on IvyBridge, Haswell (and higher) Intel processor 
systems with 2 CPU sockets or more. A proper configuration as mentioned above and 
explicit calls to NVM memory allocation in the application’s source code is required.
- The emulator will bind application threads to node 0 CPU and DRAM. The 
 other CPU socket will not be used for application threads and the DRAM 
from this second socket will be used as virtual NVM;
- The application must explicitly allocate virtual NVRAM memory using 
pmalloc(size) and pfree(pointer, size) API provided by the emulator. 

See the NVM programming section below.


NVM programming
---------------
The emulator provides an API for allocating and deallocating memory from NVM
space. It is possible to use this API on both NVM only and DRAM+NVM modes. 
However, it is really required to use this API in the DRAM+NVM mode so the 
emulator can clearly differentiate DRAM from NVM memory access latencies.
This is the API available for user applications:

    void *pmalloc(size_t size);
    void pfree(void *start, size_t size);

The application can include the NVM_EMUL/src/lib/pmalloc.h header file to
properly define these headers.
See test/test_nvm.c and test/test_nvm_remote_dram.c for an example on how to
allocate memory on respectively local DRAM or virtual NVM on a DRAM+NVM 
emulation mode.


Statistics
----------
The emulator collects statistical data to help on emulation accuracy validation.
If enabled, by default the emulator will show the statistics report when the 
user application terminates to the standard output. Some applications suppress
output to stdout, you can still see the reports by defining a target file for 
the report in the configuration file. When using a file as output, the emulator
appends the result to the file and then previous reports are not overwritten.
The statistics source code can also be statically removed at compile time. See 
Building section.

These are the reported statistics:

    - initialization duration   Time in micro seconds took by the emulator to 
                                initialize.
    - running threads           The number of threads still running. If the report
                                was called automatically by the emulator, all user 
                                threads are already terminated.
    - terminated threads        Number of terminated threads, including the main
                                thread.
    For each application thread:
    - thread id                 Thread id.
    - cpu id                    CPU id where the user thread was bind to.
    - spawn timestamp           Thread spawn timestamp as reported by the
                                monotonic time.
    - termination timestamp     Thread termination timestamp as reported by the
                                monotonic time.
    - execution time
    - stall cycles              Total number of CPU stalls caused by memory 
                                accesses made by this thread.
    - NVM accesses              Number of effective NVM accesses performed by
                                the application.
    - latency calculation overhead cycles     Overhead cycles caused by the 
                                              emulator and that could not be
                                              amortized. Zero is expected.
                                              Otherwise, consider increasing
                                              the epoch duration.
    - injected delay cycles     Total number of cycles injected by the emulator
                                to emulate the target latency.
    - injected delay in usec    Same value as above, but shown in micro seconds.
    - longest epoch duration    The effective longest epoch duration ever 
                                performed for this thread.
    - shortest epoch duration   The effective shortest epoch duration ever 
                                performed for this thread.
    - average epoch duration    The average epoch duration for this thread.
    - number of epochs          Total number of epochs performed for this 
                                thread.
    - epochs which didn't reach min duration   Number of epochs requested by 
                                               either Thread Monitor or thread 
                                               synchronizations, but were not 
                                               open since the epoch durations
                                               didn't reach the minimum epoch
                                               duration.
    - static epochs requested   Number of epochs requested by the Thread Monitor.


Support to PAPI
---------------
Performance API (PAPI) library may be used with the emulator and there are some 
hooks to switch the current CPU counters reading method to PAPI. Up to the time 
of this writing, there was no way to make PAPI CPU counter reading to perform 
at the performance level required by the emulation. In the future, if it is 
desired to switch to PAPI, follow these steps:
 - Device pmc_ioctl_setcounter() and emulator lib set_counter() in dev/pmc.c 
   calls can be deleted.
 - Define PAPI_SUPPORT for src/lib/* source code.
 - Compile with lib/cpu/pmc-papi.c rather than lib/cpu/pmc.c.
 - Link code with PAPI and add PAPI include directory.
 - Some extra tweaks may be required, check TODOs in the code.


Multiple emulated processes and MPI programs
--------------------------------------------
The emulator needs to bind user threads to specific CPU cores in order to 
optimize emulation results. It is required to export the EMUL_LOCAL_PROCESSES 
environment variable with the number or emulated processes on the host. The 
emulator will manage each emulated processes to partition the available CPUs in 
a coordinated way. It is recommended to set EMUL_LOCAL_PROCESSES with up to half 
number of available CPU cores (note DRAM+NVM mode already reserves half of 
available CPU cores).

If EMUL_LOCAL_PROCESSES is not set or set with a value lower than 2, the 
emulator will not partition CPU cores per process.

If some process crashes the emulator might not have cleaned up the environment
and the process rank ids will not be correctly managed. On this case, close all
emulated processes and delete files /tmp/emul_lock_file and 
/tmp/emul_process_local_rank if they exist.


Bandwidth emulation
-------------------
Quartz supports an emulation mode with "throttled" memory bandwidth. 

The memory bandwidth emulation  makes use of the copy kernel from the Stream benchmark, 
openMP version. When the bandwidth emulation is enabled for a first time, Quartz
creates a memory bandwidth model by utilizing the available *Thermal Registers* in the 
Memory Controller and measuring the corresponding memory bandwidth. This initial step of 
building a model might take several minutes **(~10min)**.

For the memory bandwitdh emulation, *turn off the latency modeling*
in the configuration file and select all available NUMA nodes in the 
configuration file in order to prepare the model for any combination of NUMA
nodes selection.

Modeling data will be cached to these files:

    /tmp/bandwidth_model
    /tmp/mc_pci_bus
As first step, the emulator will detect the Memory Controller Thermal Registers
Control PCI addresses and cache it to /tmp/mc/pci_bus. After this step, the 
emulator will close the current execution to safely clear NUMA bindings. Rerun
the process to resume the work. 

Quartz will create the file: **/tmp/bandwidth_model**. 

It reflects the relationship between Thermal Registers and achievable memory 
bandwidth (in a single socket). The line format in this file is:

    read <thermal register value> <memory bandwidth MB/s>
This file should present ascending values of memory bandwidth ranging from
hundreds of MiB/s to tens of GiB/S. These values (or their approximations) 
can be used for the experiments with memory bandwidth throttling. Note, that 
the model is built once: it is cached and then used for all later experiments.
(You can also run a specially prepared  automated script *bandwidth-model-building.sh* 
in directory *benchmark-tests*. For details see [README-BENCHMARKS-TESTING.md]
(https://github.hpe.com/labs/quartz/blob/master/README-BENCHMARKS-TESTING.md).

For example, to enable memory bandwidth throttling at 2 GB/s, you should change
the emulator configuration file  "nvmemul.ini" using the following settings:

    bandwidth:
    {
    enable = true;
    model = "/tmp/bandwidth_model";
    read = 2000;
    write = 2000;
    };

Both read and write bandwidth values must be set to the same value since the 
emulator does not model read/write independently in the current version. 
See Limitations session.

The pmalloc() family is not intended to be used with the bandwidth modeling. Use
numactl for instance to bind CPU and memory of the used application to the 
intended NUMA node depending. The bandwidth emulator considers the virtual NVRAM 
node only (in the configuration with two sockets). So it is required the application 
to keep processes/threads and data on the same NUMA node for bandwidth experiments.

Automated Benchmark Runs
-------------------------
We have created several automated tests with benchmark runs and output analysis 
for testing the correctness of configured emulation environment and the accuracy 
of expected results. For details see [README-BENCHMARKS-TESTING.md]
(https://github.hpe.com/labs/quartz/blob/master/README-BENCHMARKS-TESTING.md).

Limitations
-----------
The emulator functionality may be affected by certain conditions in user 
applications:
 - application sets threads CPU and memory affinity.
 - application opens much more concurrent threads than available cores per 
   socket. Note that on DRAM+NVM emulation mode, half of the available CPU 
   cores is not used for user threads.
 - application sets handler for SIGUSR1.
Other:
 - Write memory latency is not yet implemented.
 - Write/Read memory bandwidth emulation cannot be set independently.
 - The signal handler may cause syscalls in the application to fail. It is
   recommended to implement retries at the application level as a good practice 
   for syscalls.
 - Child process from fork() calls are not tracked by the emulator. As a
   workaround, the emulator could make the library initialization function 
   available in the external API. Applications then should call this function
   in the beginning of the child process.
 - OpenMP applications may use synchronization primitives not based on
   pthreads which are currently not supported.
 - See Todo session for details.


Todo list
---------
Please see accompanied TODO.dox or extended documentation for an extensive 
list.

#License

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

    
#Copyright

	    (c) Copyright 2016 Hewlett Packard Enterprise Development LP

**NOTE**: This software depends on other packages that may be licensed under different open source licenses.

