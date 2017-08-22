option(STATISTICS "Enable statistics report" ON)

if(STATISTICS)
  message(STATUS "WITH STATISTICS")
  add_definitions(-DUSE_STATISTICS)
else()
  message(STATUS "WITHOUT STATISTICS")
endif()

set(quartzcore_src
    config.c
    dev.c
    debug.c
    graph.c
    fam_atomic.c
    libpthread.c
    measure_bw.c
    measure_lat.c
    model_bw.c
    model_lat.c
    monotonic_timer.c
    misc.c
    pflush.c
    queue.c
    stat.c
    thread.c
    throttle.c
    topology.c
)

set(nvmemul_src
    init.c
    interpose.c
)

include_directories(${LIBXML_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/third_party)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/lib)
add_definitions(-g)
add_definitions(-O2)
add_definitions(-fPIC)
add_definitions(-Wall)
add_definitions(-march=native)
add_definitions(-fopenmp)
add_definitions(-std=gnu89)
#add_definitions(-DNDEBUG)
#add_definitions(-std=c99)
add_definitions(-msse4)
add_subdirectory(cpu)
add_library(quartzcore SHARED ${quartzcore_src} cpu/cpu.c cpu/pmc.c)
target_link_libraries(quartzcore config)
target_link_libraries(quartzcore m)
target_link_libraries(quartzcore numa)
target_link_libraries(quartzcore gomp)
target_link_libraries(quartzcore ${LIBXML_LIBRARY})
add_library(nvmemul SHARED ${nvmemul_src})
target_link_libraries(nvmemul quartzcore)
target_link_libraries(nvmemul dl)
target_link_libraries(nvmemul rt)
