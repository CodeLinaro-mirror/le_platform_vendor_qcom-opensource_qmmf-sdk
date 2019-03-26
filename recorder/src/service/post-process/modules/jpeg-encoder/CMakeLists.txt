cmake_minimum_required(VERSION 3.1)

project(qmmf_jpeg)

if (NOT RECORDER_SERVICE_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_jpeg SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_jpeg.cc
)
add_dependencies(qmmf_jpeg qmmf_common_jpeg_encoder qmmf_utils)

target_include_directories(qmmf_jpeg
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_jpeg
 PRIVATE ${TOP_DIRECTORY}/recorder/src/service/post-process)

target_include_directories(qmmf_jpeg
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_jpeg DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_jpeg qmmf_common_jpeg_encoder qmmf_utils jsoncpp log utils cutils dl camera_metadata camera_client)