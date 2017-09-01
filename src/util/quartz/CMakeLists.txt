set(src
    cmd_create.c
    cmd_destroy.c
    cmd_discover.c
    cmd_show.c
	common.c
    main.c
)

#add_definitions(-std=gnu89)
#add_definitions(-DNDEBUG)
#add_definitions(-std=c99)
add_definitions(-g)

include_directories(${LIBXML_INCLUDE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/src/lib)
include_directories(${CMAKE_SOURCE_DIR})
add_executable(quartz ${src})
target_link_libraries(quartz quartzcore)
