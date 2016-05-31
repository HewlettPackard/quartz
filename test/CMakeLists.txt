include_directories(${CMAKE_SOURCE_DIR}/third_party/gtest-1.7.0/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib)

add_definitions(-g)
add_definitions(-Wall)
#add_definitions(-DNDEBUG)

add_executable(test_interpose ${CMAKE_CURRENT_SOURCE_DIR}/test_interpose.cc)
target_link_libraries(test_interpose pthread gtest)

add_executable(test_dev ${CMAKE_CURRENT_SOURCE_DIR}/test_dev.cc)
target_link_libraries(test_dev pthread nvmemul)

add_executable(test_thread ${CMAKE_CURRENT_SOURCE_DIR}/test_thread.cc)
target_link_libraries(test_thread nvmemul pthread)

add_executable(test_mutex ${CMAKE_CURRENT_SOURCE_DIR}/test_mutex.cc)
target_link_libraries(test_mutex nvmemul pthread)

add_executable(test_nvm_remote_dram ${CMAKE_CURRENT_SOURCE_DIR}/test_nvm_remote_dram.c)
target_link_libraries(test_nvm_remote_dram nvmemul)

add_executable(test_nvm ${CMAKE_CURRENT_SOURCE_DIR}/test_nvm.c)
target_link_libraries(test_nvm nvmemul)

add_executable(test_multithread ${CMAKE_CURRENT_SOURCE_DIR}/test_multithread.c)
#target_link_libraries(test_multithread rt)
target_link_libraries(test_multithread nvmemul pthread)

add_test(NAME interpose COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_interpose)

set(ENV_COMMON "LD_PRELOAD=${CMAKE_BINARY_DIR}/src/emul/libnvmemul.so")

SET_PROPERTY(TEST interpose PROPERTY ENVIRONMENT ${ENV_COMMON} "ENUM_INI=emul.ini")
