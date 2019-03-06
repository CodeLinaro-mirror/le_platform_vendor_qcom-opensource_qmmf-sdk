cmake_minimum_required(VERSION 3.1)

include_directories(${SYSROOT_INCDIR})
include_directories(${SYSROOT_INCDIR}/mm-core)
link_directories(${SYSROOT_LIBDIR})

project(qmmf-sdk)

set(CMAKE_CXX_STANDARD 14)

set(TOP_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

include_directories(${TOP_DIRECTORY}/include)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -fno-short-enums -O2 -DANDROID")
# uncomment to generate debug information
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -Wl,-no-undefined")
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-no-undefined")
set(CMAKE_STATIC_LINKER_FLAGS
    "${CMAKE_STATIC_LINKER_FLAGS} -Wl,-no-undefined")

add_subdirectory(config/LE-${TARGET_BOARD_PLATFORM})

foreach(l ${PLATFORM_INCLUDES})
include_directories(${l})
endforeach()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PLATFORM_CXX_FLAGS}")

add_subdirectory(qmmf-server)

add_subdirectory(plugins)

add_subdirectory(player/src/service)

add_subdirectory(player/test/gtest)

add_subdirectory(player/test/samples)

add_subdirectory(player/test/screen-nail)

add_subdirectory(player/test/transcode_app)

add_subdirectory(player/test/demuxer)

add_subdirectory(player/src/client)

add_subdirectory(recorder/src/service)

add_subdirectory(recorder/test/gtest)

add_subdirectory(recorder/test/samples)

add_subdirectory(recorder/test/videotimelapse)

add_subdirectory(recorder/test/timelapse)

add_subdirectory(recorder/src/client)

add_subdirectory(display/test/gtest)

add_subdirectory(display/test/samples)

add_subdirectory(display/src/service)

add_subdirectory(display/src/client)

add_subdirectory(avcodec/test/gtest)

add_subdirectory(avcodec/test/sample)

add_subdirectory(avcodec/src)

add_subdirectory(system/test/samples)

add_subdirectory(system/src/service)

add_subdirectory(system/src/client)

add_subdirectory(common/memory)

add_subdirectory(common/audio/src/service)

add_subdirectory(common/audio/test/samples)

add_subdirectory(common/audio/src/client)

add_subdirectory(common/avqueue)

add_subdirectory(common/cameraadaptor/gtest)

add_subdirectory(common/cameraadaptor)

add_subdirectory(common/codecadaptor/src)

add_subdirectory(common/exif-generator)

add_subdirectory(common/jpeg-encoder)

add_subdirectory(common/overlay/test)

add_subdirectory(common/overlay)

add_subdirectory(common/resizer-c2d)

add_subdirectory(common/resizer-fastCV)

add_subdirectory(common/resizer-neon)

add_subdirectory(common/utils)