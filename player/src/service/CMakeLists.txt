cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_PLAYER_SERVICE)

project(qmmf_player_service)

if (NOT PLAYER_SERVICE_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_player_service SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_player_service.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_player_impl.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_player_common.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_player_ion.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_player_remote_cb.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_player_audio_decoder_core.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_player_video_decoder_core.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_player_audio_sink.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_player_audio_raw_sink.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_player_video_sink.cc
)
add_dependencies(qmmf_player_service qmmf_player_client qmmf_utils qmmf_codec_adaptor qmmf_audio_client qmmf_display_client)
if (NOT DISABLE_PP_JPEG)
add_dependencies(qmmf_player_service qmmf_common_jpeg_encoder)
endif()

target_include_directories(qmmf_player_service
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_player_service
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_include_directories(qmmf_player_service
 PRIVATE ${PKG_CONFIG_SYSROOT_DIR}/usr/include/ion_headers)

target_include_directories(qmmf_player_service
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_player_service DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_player_service qmmf_utils qmmf_player_client
qmmf_audio_client qmmf_codec_adaptor pthread log binder utils
cutils fastcvopt qmmf_display_client)

endif()