include_directories(${CMAKE_SOURCE_DIR}/src/lib)
add_executable(new_memlat memlat.c)
target_link_libraries(new_memlat nvmemul pthread)
