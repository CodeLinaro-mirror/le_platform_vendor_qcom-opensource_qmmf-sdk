cmake_minimum_required(VERSION 3.1)

project(qmmf_postproc_frame_skip)

if (NOT RECORDER_SERVICE_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_postproc_frame_skip SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_postproc_frame_skip.cc
)

target_include_directories(qmmf_postproc_frame_skip
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_postproc_frame_skip
 PRIVATE ${TOP_DIRECTORY}/recorder/src/service/post-process)

target_include_directories(qmmf_postproc_frame_skip
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_postproc_frame_skip DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_postproc_frame_skip jsoncpp log utils cutils dl pthread binder)