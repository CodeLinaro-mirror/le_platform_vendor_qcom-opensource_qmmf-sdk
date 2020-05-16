cmake_minimum_required(VERSION 3.1)

project(qmmf_camera_adaptor)

if (NOT CAMERAADAPTOR_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_camera_adaptor SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera3_device_client.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera3_monitor.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera3_request_handler.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera3_prepare_handler.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera3_stream.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera3_utils.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera3_smooth_zoom.cc
)
add_dependencies(qmmf_camera_adaptor qmmf_utils)

target_include_directories(qmmf_camera_adaptor
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_camera_adaptor
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_camera_adaptor
 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(qmmf_camera_adaptor
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_camera_adaptor DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_camera_adaptor camera_metadata log binder qmmf_utils utils
cutils pthread dl hardware qmmf_memory_interface)
if(NOT CAMERA_CLIENT_DISABLED)
target_link_libraries(qmmf_camera_adaptor camera_client)
endif()
if(TARGET_USES_GBM)
target_link_libraries(qmmf_camera_adaptor gbm)
endif()
