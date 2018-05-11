cmake_minimum_required(VERSION 2.8)

project(quartz)


set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

#find_package(Numa REQUIRED)
find_package(Libxml REQUIRED)

add_subdirectory(src)
add_subdirectory(bench)
enable_testing()
add_subdirectory(test)
