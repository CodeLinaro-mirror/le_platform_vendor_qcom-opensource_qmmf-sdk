cmake_minimum_required(VERSION 3.1)

project(qmmf_camera_hal_reproc)

if (NOT RECORDER_SERVICE_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_camera_hal_reproc SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera_hal_reproc.cc
)
add_dependencies(qmmf_camera_hal_reproc qmmf_memory_interface)

target_include_directories(qmmf_camera_hal_reproc
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_camera_hal_reproc
 PRIVATE ${TOP_DIRECTORY}/recorder/src/service/post-process)

target_include_directories(qmmf_camera_hal_reproc
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_camera_hal_reproc DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_camera_hal_reproc log utils cutils binder camera_client camera_metadata qmmf_memory_interface)