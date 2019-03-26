cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_PLAYER_SERVICE)

project(qmmf_demuxer)

if (NOT PLAYER_DEMUXER_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_demuxer SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_demuxer_sourceport.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_demuxer_intf.cc
)

target_include_directories(qmmf_demuxer
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_demuxer
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

install(TARGETS qmmf_demuxer DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_demuxer log utils cutils pthread dl mmosal mmparser_lite)

endif()