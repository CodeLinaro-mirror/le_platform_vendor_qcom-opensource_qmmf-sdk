cmake_minimum_required(VERSION 3.1)

project(qmmf_audio_service)

if (NOT AUDIO_SERVICE_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_audio_service SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_service.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_ion.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_frontend.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_backend_source.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_audio_backend_sink.cc
)
add_dependencies(qmmf_audio_service qmmf_audio_client)

target_include_directories(qmmf_audio_service
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_audio_service
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_audio_service
 PRIVATE ${TOP_DIRECTORY}/common/memory)

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_include_directories(qmmf_audio_service
 PRIVATE ${PKG_CONFIG_SYSROOT_DIR}/usr/include/ion_headers)

install(TARGETS qmmf_audio_service DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_audio_service qmmf_audio_client log binder utils
 cutils qahw pthread)

 # TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_link_libraries(qmmf_audio_service ion)
