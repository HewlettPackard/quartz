set(src
    cmd_make.c
    main.c
)

include_directories(${CMAKE_SOURCE_DIR})
add_executable(quartz ${src})
target_link_libraries(quartz nvmemul pthread)
