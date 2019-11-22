cmake_minimum_required(VERSION 3.1)

project(qmmf-server)

if (NOT QMMF_SERVER_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHFR_THRESHOLD=30 -DAEC_WAIT_TIMEOUT=500000000")

add_executable(qmmf-server ${exclude} qmmf_server_main.cc)
if (NOT CAMERA_HAL1_SUPPORT)
add_dependencies(qmmf-server qmmf_audio_service qmmf_recorder_service)
else()
add_dependencies(qmmf-server qmmf_recorder_service)
endif()
if(NOT DISABLE_DISPLAY)
add_dependencies(qmmf-server qmmf_display_service)
endif()

target_include_directories(qmmf-server
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf-server
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf-server
 PRIVATE ${WORKSPACE}/hardware/qcom/media/)

target_include_directories(qmmf-server
 PRIVATE ${TOP_DIRECTORY}/recorder/src/service/post-process)

target_include_directories(qmmf-server
 PRIVATE ${TOP_DIRECTORY}/common/memory)

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_include_directories(qmmf-server
 PRIVATE ${PKG_CONFIG_SYSROOT_DIR}/usr/include/ion_headers)

install(TARGETS qmmf-server DESTINATION bin OPTIONAL)
if (NOT CAMERA_HAL1_SUPPORT)
target_link_libraries(qmmf-server log cutils utils binder qmmf_audio_service qmmf_recorder_service)
else()
target_link_libraries(qmmf-server log cutils utils binder qmmf_recorder_service)
endif()
if(NOT DISABLE_DISPLAY)
target_link_libraries(qmmf-server qmmf_display_service)
endif()

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_link_libraries(qmmf-server ion)

if (TARGET_BOARD_PLATFORM STREQUAL "apq8009")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/service/qmmf-server-cam-arch-v1.service DESTINATION ${QMMF_SYSTEMD_DIR} RENAME qmmf-server.service)
elseif (TARGET_BOARD_PLATFORM STREQUAL "apq8053")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/service/qmmf-server-cam-arch-v1.service DESTINATION ${QMMF_SYSTEMD_DIR} RENAME qmmf-server.service)
elseif (TARGET_BOARD_PLATFORM STREQUAL "qcs605")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/service/qmmf-server-cam-arch-v2.service DESTINATION ${QMMF_SYSTEMD_DIR} RENAME qmmf-server.service)
elseif (TARGET_BOARD_PLATFORM STREQUAL "sdmsteppe")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/service/qmmf-server-cam-arch-v2-usrgrp.service DESTINATION ${QMMF_SYSTEMD_DIR} RENAME qmmf-server.service)
else()
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/service/qmmf-server-base.service DESTINATION ${QMMF_SYSTEMD_DIR} RENAME qmmf-server.service)
endif()
