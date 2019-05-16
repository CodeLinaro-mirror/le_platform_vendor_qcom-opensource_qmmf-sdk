cmake_minimum_required(VERSION 3.1)

project(qmmf_av_codec)

if (NOT AVCODEC_LIB_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
else()
#EXPORT_HEADERS
install(FILES
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_avcodec.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_avcodec_params.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_buffer.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_codec.h
DESTINATION include/qmmf-sdk)
endif()

add_library(qmmf_av_codec SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_avcodec.cc
)

add_dependencies(qmmf_av_codec qmmf_utils qmmf_codec_adaptor)
if (NOT DISABLE_PP_JPEG)
add_dependencies(qmmf_av_codec qmmf_common_jpeg_encoder)
endif()

target_include_directories(qmmf_av_codec
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_av_codec
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_av_codec
 PRIVATE ${TOP_DIRECTORY}/common/memory)

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_include_directories(qmmf_av_codec
 PRIVATE ${PKG_CONFIG_SYSROOT_DIR}/usr/include/ion_headers)

install(TARGETS qmmf_av_codec DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_av_codec qmmf_codec_adaptor qmmf_utils utils cutils pthread dl log)

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_link_libraries(qmmf_av_codec ion)

if (NOT DISABLE_PP_JPEG)
target_link_libraries(qmmf_av_codec qmmf_common_jpeg_encoder)
endif()