cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_AVCODEC_TEST)

project(qmmf_av_codec_gtest)

if (NOT AVCODEC_GTEST_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_executable(qmmf_av_codec_gtest ${exclude} qmmf_avcodec_gtest.cc)
add_dependencies(qmmf_av_codec_gtest qmmf_av_codec qmmf_utils)

target_include_directories(qmmf_av_codec_gtest
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_av_codec_gtest
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_av_codec_gtest
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_av_codec_gtest DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_av_codec_gtest log cutils utils qmmf_utils gtest gtest_main qmmf_av_codec)

endif()