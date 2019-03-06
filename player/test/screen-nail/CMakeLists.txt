cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_SCREENNAIL_APP)

project(qmmf_screen_nail)

if (NOT PLAYER_SCREEN_NAIL_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_executable(qmmf_screen_nail ${exclude} qmmf_screen_nail_app.cc)
add_dependencies(qmmf_screen_nail qmmf_demuxer qmmf_av_codec)

target_include_directories(qmmf_screen_nail
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_screen_nail
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_screen_nail
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_screen_nail DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_screen_nail log cutils utils pthread fastcvopt qmmf_demuxer qmmf_av_codec)
if(TARGET_USES_GBM)
target_link_libraries(qmmf_screen_nail gbm)
endif()

endif()