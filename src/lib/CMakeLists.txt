project(nvmemul)

option(STATISTICS "Enable statistics report" ON)

if(STATISTICS)
  message(STATUS "WITH STATISTICS")
  add_definitions(-DUSE_STATISTICS)
else()
  message(STATUS "WITHOUT STATISTICS")
endif()

set(nvmemul_src
    config.c
    debug.c
    dev.c
    init.c
    interpose.c
    measure_bw.c
    measure_lat.c
    misc.c
    monotonic_timer.c
    model_bw.c
    model_lat.c
    pflush.c
    pmalloc.c
    stat.c
    thread.c
    topology.c
    process_rank.c
)

include_directories(${CMAKE_SOURCE_DIR}/third_party)
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
add_library(nvmemul SHARED ${nvmemul_src} $<TARGET_OBJECTS:cpu>)
target_link_libraries(nvmemul dl)
target_link_libraries(nvmemul config)
target_link_libraries(nvmemul numa)
target_link_libraries(nvmemul rt)
target_link_libraries(nvmemul m)
target_link_libraries(nvmemul gomp)
