cmake_minimum_required(VERSION 3.1)

if (NOT DISABLE_VIDEO_TIMELAPSE)

project(qmmf_recorder_video_timelapse)

if (NOT RECORDER_VIDEOTIMELAPSE_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_executable(qmmf_recorder_video_timelapse ${exclude} qmmf_video_time_lapse.cc qmmf_video_time_lapse_main.cc)
add_dependencies(qmmf_recorder_video_timelapse qmmf_utils qmmf_recorder_client qmmf_av_codec)

target_include_directories(qmmf_recorder_video_timelapse
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_recorder_video_timelapse
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

install(TARGETS qmmf_recorder_video_timelapse DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_recorder_video_timelapse log cutils utils camera_metadata qmmf_recorder_client qmmf_utils qmmf_av_codec)
if(NOT CAMERA_CLIENT_DISABLED)
target_link_libraries(qmmf_recorder_video_timelapse camera_client)
endif()
endif()
