cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_TRANSCODE_APP)

project(qmmf_transcode_app)

if (NOT PLAYER_TRANSCODE_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_executable(qmmf_transcode_app ${exclude}
qmmf_transcode_utils.cc
qmmf_transcode_pipe.cc
qmmf_transcode_source.cc
qmmf_transcode_sink.cc
qmmf_transcode_track.cc
qmmf_transcode_test.cc)
add_dependencies(qmmf_transcode_app qmmf_demuxer qmmf_av_codec)

target_include_directories(qmmf_transcode_app
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_transcode_app
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_transcode_app
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_transcode_app DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_transcode_app log cutils utils qmmf_demuxer qmmf_av_codec pthread)
if(TARGET_USES_GBM)
target_link_libraries(qmmf_transcode_app gbm)
endif()

endif()