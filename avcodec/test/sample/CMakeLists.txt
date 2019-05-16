cmake_minimum_required(VERSION 3.1)

project(qmmf_av_codec_sample)

if (NOT AVCODEC_SAMPLES_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

if (NOT DISABLE_AVCODEC_TEST)

add_executable(qmmf_av_codec_test ${exclude} qmmf_avcodec_test.cc)
add_dependencies(qmmf_av_codec_test qmmf_av_codec)

target_include_directories(qmmf_av_codec_test
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_av_codec_test
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_av_codec_test
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_av_codec_test DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_av_codec_test log cutils qmmf_utils utils qmmf_av_codec hardware)
if(TARGET_USES_GBM)
target_link_libraries(qmmf_av_codec_test gbm)
endif()

endif()

if (NOT DISABLE_AVCODEC_AUDIO_DECODE_TEST)

add_executable(qmmf_av_codec_audio_decode_test ${exclude} qmmf_audio_decode_test.cc)
add_dependencies(qmmf_av_codec_audio_decode_test qmmf_av_codec)

target_include_directories(qmmf_av_codec_audio_decode_test
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_av_codec_audio_decode_test
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_av_codec_audio_decode_test
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_av_codec_audio_decode_test DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_av_codec_audio_decode_test log cutils qmmf_utils utils qmmf_av_codec)

endif()

if (NOT DISABLE_AVCODEC_AUDIO_ENCODE_TEST)

add_executable(qmmf_av_codec_audio_encode_test ${exlude} qmmf_audio_encode_test.cc)
add_dependencies(qmmf_av_codec_audio_encode_test qmmf_av_codec)

target_include_directories(qmmf_av_codec_audio_encode_test
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_av_codec_audio_encode_test
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

install(TARGETS qmmf_av_codec_audio_encode_test DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_av_codec_audio_encode_test log cutils qmmf_utils utils qmmf_av_codec)

endif()

if(ENABLE_JPEG_TURBO)
add_executable(qmmf_jpeg_turbo_decode_test ${exclude} jpeg-turbo/jpeg_turbo_decode_test.cc)
add_dependencies(qmmf_jpeg_turbo_decode_test qmmf_av_codec)

target_include_directories(qmmf_jpeg_turbo_decode_test
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_jpeg_turbo_decode_test
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

install(TARGETS qmmf_jpeg_turbo_decode_test DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_jpeg_turbo_decode_test log cutils qmmf_utils utils qmmf_av_codec jpeg turbojpeg)
endif()