cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_DISPLAY_CLIENT)

project(qmmf_display_client)

if (ENABLE_WESTON_CLIENT)
set(OUTPUT_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/gbm-buffer-backend-client-protocol.h")
set(WAYLAND_BINARY "${TOP_DIRECTORY}/../recipe-sysroot-native/usr/bin/wayland-scanner")

add_custom_command(OUTPUT ${OUTPUT_HEADER}
COMMAND ${WAYLAND_BINARY} client-header < ${WORKSPACE}/display/weston/protocol/gbm-buffer-backend.xml > ${OUTPUT_HEADER}
WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(header
DEPENDS ${OUTPUT_HEADER})
endif()

if (NOT DISPLAY_CLIENT_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
else()
#EXPORT_HEADERS
install(FILES
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_recorder.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_recorder_params.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_display.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_display_params.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_system.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_system_params.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_player.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_player_params.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_device.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_codec.h
 DESTINATION include/qmmf-sdk)
endif()

if (NOT ENABLE_WESTON_CLIENT)
add_library(qmmf_display_client SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_display.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_display_client.cc
)
else()
add_library(qmmf_display_client SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_display.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_display_weston_client.cc
)
add_dependencies(qmmf_display_client header)
endif()

target_include_directories(qmmf_display_client
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_display_client
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_display_client
 PRIVATE ${TOP_DIRECTORY}/common/memory)

target_include_directories(qmmf_display_client
 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(qmmf_display_client
 PRIVATE ${PKG_CONFIG_SYSROOT_DIR}/usr/include/ion_headers)

install(TARGETS qmmf_display_client DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_display_client log binder utils cutils)
if (ENABLE_WESTON_CLIENT)
target_link_libraries(qmmf_display_client pthread wayland-client gbm-buffer-backend-protocol)
endif()
if (TARGET_USES_GBM)
target_link_libraries(qmmf_display_client gbm)
endif()

endif()
