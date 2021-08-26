cmake_minimum_required(VERSION 3.1)
project(qmmf_recorder_timelapse)

if (NOT RECORDER_TIMELAPSE_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_executable(qmmf_recorder_timelapse ${exclude} qmmf_time_lapse.cc qmmf_time_lapse_main.cc)
add_dependencies(qmmf_recorder_timelapse qmmf_recorder_client qmmf_utils)

target_include_directories(qmmf_recorder_timelapse
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_recorder_timelapse
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_recorder_timelapse
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_recorder_timelapse DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_recorder_timelapse log cutils utils camera_metadata qmmf_recorder_client qmmf_utils)
if(NOT CAMERA_CLIENT_DISABLED)
target_link_libraries(qmmf_recorder_timelapse camera_client)
endif()
