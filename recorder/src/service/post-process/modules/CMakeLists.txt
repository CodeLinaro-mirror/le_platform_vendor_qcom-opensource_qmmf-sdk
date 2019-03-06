cmake_minimum_required(VERSION 3.1)

include_directories(${SYSROOT_INCDIR})
link_directories(${SYSROOT_LIBDIR})

project(modules)

add_subdirectory(algo)
add_subdirectory(camera-hal-reproc)
add_subdirectory(frame-skip)
add_subdirectory(test)
if (NOT DISABLE_PP_JPEG)
add_subdirectory(jpeg-encoder)
endif()