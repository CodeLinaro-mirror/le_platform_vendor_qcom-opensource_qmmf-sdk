cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_SYSTEM_SERVICE)

project(qmmf_system_service)

if (NOT SYSTEM_SERVICE_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_system_service SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_system_service.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_system_implementation.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_system_ion.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_system_keytone.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_system_devices.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_system_trigger.cc
)
add_dependencies(qmmf_system_service qmmf_audio_client qmmf_system_client)

target_include_directories(qmmf_system_service
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_system_service
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_system_service
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_system_service DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_system_service qmmf_system_client qmmf_audio_client log binder utils cutils
qtisoundtriggerhw pthread)

endif()