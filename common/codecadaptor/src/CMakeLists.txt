cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDISABLE_VID_QP_RANGE -DDISABLE_VID_LPM")

project(qmmf_codec_adaptor)

if (NOT CODECADAPTOR_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

if (NOT DISABLE_PP_JPEG)
set(jpeg ${CMAKE_CURRENT_SOURCE_DIR}/qmmf_jpeg_encode.cc)
endif()

add_library(qmmf_codec_adaptor SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_omx_client.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_avcodec.cc
${jpeg}
)

if (NOT DISABLE_PP_JPEG)
add_dependencies(qmmf_codec_adaptor qmmf_common_jpeg_encoder)
endif()
add_dependencies(qmmf_codec_adaptor qmmf_utils)

target_include_directories(qmmf_codec_adaptor
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_codec_adaptor
 PRIVATE ${TOP_DIRECTORY}/common)

target_include_directories(qmmf_codec_adaptor
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_codec_adaptor
 PRIVATE ${TOP_DIRECTORY}/common/memory)

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_include_directories(qmmf_codec_adaptor
 PRIVATE ${PKG_CONFIG_SYSROOT_DIR}/usr/include/ion_headers)

install(TARGETS qmmf_codec_adaptor DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_codec_adaptor qmmf_utils utils cutils pthread dl log hardware camera_metadata)

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_link_libraries(qmmf_codec_adaptor ion)

if (NOT DISABLE_PP_JPEG)
target_link_libraries(qmmf_codec_adaptor qmmf_common_jpeg_encoder)
endif()