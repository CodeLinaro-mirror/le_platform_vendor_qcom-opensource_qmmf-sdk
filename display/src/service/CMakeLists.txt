cmake_minimum_required(VERSION 3.1)

if (NOT DISABLE_DISPLAY)

project(qmmf_display_service)

if (NOT DISPLAY_SERVICE_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_display_service SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_display_service.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_display_impl.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_remote_cb.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_display_sdm_buffer_allocator.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_display_sdm_debugger.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_display_sdm_buffer_sync_handler.cc
)
add_dependencies(qmmf_display_service qmmf_display_client qmmf_camera_adaptor qmmf_utils)

target_include_directories(qmmf_display_service
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_display_service
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_display_service
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_display_service DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_display_service qmmf_display_client qmmf_camera_adaptor qmmf_utils log binder utils cutils sdmcore sdmutils memalloc sync pthread dl)

endif()