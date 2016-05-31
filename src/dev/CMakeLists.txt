# Build NVM Emulation device driver (using Kbuild Makefile)

set(DEV_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(DEV_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(DEV_KERNEL_MODULE "${DEV_BIN_DIR}/nvmemul.ko")
mark_as_advanced(DEV_DIR DEV_BIN_DIR)

# We invoke make in build folder to keep the glog's source folder clean.
file(MAKE_DIRECTORY ${DEV_BIN_DIR})
add_custom_command(OUTPUT ${DEV_KERNEL_MODULE}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${DEV_DIR} ${DEV_BIN_DIR}
    COMMAND ${CMAKE_MAKE_PROGRAM} -j
    COMMENT [Build-NVM Emulation Device]
    WORKING_DIRECTORY "${DEV_BIN_DIR}"
    DEPENDS ${DEV_DIR}/pmc.c # just to see if it has been overwritten
)

# we use add_custom_command for the build itself because otherwise we have to build it
# every time. the following add_custom_target gives a name for the output.
add_custom_target(dev_build ALL DEPENDS ${DEV_KERNEL_MODULE})
