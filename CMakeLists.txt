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

install(FILES
    ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_player_params.h
DESTINATION include/qmmf-sdk)

add_subdirectory(qmmf-server)

add_subdirectory(recorder/src/service)

add_subdirectory(recorder/test/gtest)

add_subdirectory(recorder/test/audio)

if (NOT CAMERA_HAL1_SUPPORT)

add_subdirectory(recorder/test/videotimelapse)

add_subdirectory(recorder/test/timelapse)
endif()

add_subdirectory(recorder/src/client)

add_subdirectory(test)

add_subdirectory(avcodec/test/gtest)

add_subdirectory(avcodec/test/sample)

add_subdirectory(avcodec/src)

add_subdirectory(common/memory)

add_subdirectory(common/avqueue)
if (NOT CAMERA_HAL1_SUPPORT)
add_subdirectory(common/cameraadaptor/gtest)

add_subdirectory(common/cameraadaptor)
endif()
add_subdirectory(common/codecadaptor/src)

add_subdirectory(common/exif-generator)

add_subdirectory(common/jpeg-encoder)

if (RESIZER_FASTCV_ENABLED)
add_subdirectory(common/resizer-fastCV)
endif()

add_subdirectory(common/resizer-c2d)

if (RESIZER_NEON_ENABLED)
add_subdirectory(common/resizer-neon)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_RESCALER_NEON")
endif()

add_subdirectory(common/utils)
