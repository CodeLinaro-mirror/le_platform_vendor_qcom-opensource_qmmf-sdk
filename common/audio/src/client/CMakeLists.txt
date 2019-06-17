cmake_minimum_required(VERSION 3.1)

project(qmmf_audio_client)

if (NOT AUDIO_CLIENT_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_audio_client SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_endpoint.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_endpoint_client.cc
)

target_include_directories(qmmf_audio_client
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_audio_client
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_audio_client
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_audio_client DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_audio_client log binder utils cutils)