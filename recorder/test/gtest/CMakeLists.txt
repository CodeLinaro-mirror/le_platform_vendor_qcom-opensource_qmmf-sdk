cmake_minimum_required(VERSION 3.1)

project(recorder-gtest)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CAIRO -D_FILE_OFFSET_BITS=64")

if (NOT RECORDER_GTEST_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

if (NOT CAMERA_HAL1_SUPPORT)
add_executable(qmmf_recorder_gtest ${exclude}
qmmf_recorder_base_gtest.cc
qmmf_recorder_gtest_common.cc
qmmf_recorder_image_gtest.cc
qmmf_recorder_video_gtest.cc
qmmf_recorder_video_snapshot_gtest.cc
)
else()
add_executable(qmmf_recorder_gtest ${exclude}
qmmf_recorder_base_gtest.cc
qmmf_recorder_hal1_gtest.cc
qmmf_recorder_gtest_common.cc
)
endif()
add_dependencies(qmmf_recorder_gtest qmmf_recorder_client qmmf_utils qmmf_av_queue)
if (NOT DISABLE_DISPLAY_CLIENT)
add_dependencies(qmmf_recorder_gtest qmmf_display_client)
endif()

target_include_directories(qmmf_recorder_gtest
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_recorder_gtest
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_recorder_gtest
 PRIVATE ${TOP_DIRECTORY}/recorder/test/gtest)

target_include_directories(qmmf_recorder_gtest
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_recorder_gtest DESTINATION bin OPTIONAL)

install(FILES imx577_sensor_mode_config.json DESTINATION ${QMMF_DATA})

target_link_libraries(qmmf_recorder_gtest log cutils utils cairo gtest gtest_main camera_client camera_metadata qmmf_recorder_client qmmf_av_queue jsoncpp)
if (NOT DISABLE_DISPLAY_CLIENT)
target_link_libraries(qmmf_recorder_gtest qmmf_display_client)
endif()