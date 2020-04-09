cmake_minimum_required(VERSION 3.1)

project(qmmf_recorder_service)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHFR_THRESHOLD=30 -DAEC_WAIT_TIMEOUT=500000000")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++11-narrowing")

if (NOT RECORDER_SERVICE_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

if (NOT CAMERA_HAL1_SUPPORT)
add_library(qmmf_recorder_service SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_recorder_service.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_recorder_impl.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_recorder_ion.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_remote_cb.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera_source.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera_context.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_encoder_core.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_pulse_client.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_source.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_raw_track_source.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_encoded_track_source.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_encoder_core.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera_rescaler.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera_frc.cc
${jpeg}
)
else()
add_library(qmmf_recorder_service SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_recorder_service.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_recorder_impl.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_recorder_ion.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_remote_cb.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera_source.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera_context_hal1.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_encoder_core.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_pulse_client.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_source.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_raw_track_source.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_encoded_track_source.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_encoder_core.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera_rescaler.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_camera_frc.cc
${jpeg}
)
endif()
if (NOT CAMERA_HAL1_SUPPORT)
add_dependencies(
  qmmf_recorder_service
  qmmf_utils
  qmmf_codec_adaptor
  qmmf_camera_adaptor
  qmmf_exif_generator
  qmmf_common_resizer_neon
  qmmf_common_resizer_c2d
  qmmf_common_resizer_fastcv
  qmmf_recorder_client
)
else()
add_dependencies(
  qmmf_recorder_service
  qmmf_utils
  qmmf_codec_adaptor
  qmmf_exif_generator
  qmmf_common_resizer_neon
  qmmf_common_resizer_c2d
  qmmf_recorder_client
)
endif()
if (NOT DISABLE_RESCALER_COLORSPACE)
add_dependencies(qdMetaData)
endif()
if (NOT DISABLE_PP_JPEG)
add_dependencies(qmmf_recorder_service qmmf_common_jpeg_encoder qmmf_jpeg)
endif()

target_include_directories(qmmf_recorder_service
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_recorder_service
 PRIVATE ${WORKSPACE}/hardware/qcom/media/)

target_include_directories(qmmf_recorder_service
 PRIVATE ${WORKSPACE}/hardware/libhardware/include)

target_include_directories(qmmf_recorder_service
 PRIVATE ${WORKSPACE}/frameworks/av/include)

target_include_directories(qmmf_recorder_service
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_recorder_service
 PRIVATE ${TOP_DIRECTORY}/common/memory)

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_include_directories(qmmf_recorder_service
 PRIVATE ${PKG_CONFIG_SYSROOT_DIR}/usr/include/ion_headers)

install(TARGETS qmmf_recorder_service DESTINATION lib OPTIONAL)
if (NOT CAMERA_HAL1_SUPPORT)
target_link_libraries(qmmf_recorder_service dl jsoncpp log utils cutils binder
  pthread pulse camera_client camera_metadata hardware qmmf_recorder_client
  qmmf_utils qmmf_camera_adaptor qmmf_codec_adaptor
  qmmf_exif_generator qmmf_common_resizer_neon
  qmmf_common_resizer_c2d qmmf_common_resizer_fastcv)
else()
target_link_libraries(qmmf_recorder_service dl jsoncpp log utils cutils binder
  pthread pulse camera_client camera_metadata hardware qmmf_recorder_client
  qmmf_utils qmmf_codec_adaptor qmmf_exif_generator qmmf_memory_interface
  qmmf_common_resizer_neon qmmf_common_resizer_c2d)
endif()
if (NOT DISABLE_PP_JPEG)
target_link_libraries(qmmf_recorder_service qmmf_common_jpeg_encoder qmmf_jpeg)
endif()

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_link_libraries(qmmf_recorder_service ion)
