include_directories(${CMAKE_SOURCE_DIR}/src/lib)

add_executable(multilat multilat.c)
target_link_libraries(multilat nvmemul pthread)
