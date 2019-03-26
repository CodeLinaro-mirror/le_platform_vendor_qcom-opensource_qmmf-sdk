cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_SYSTEM_CLIENT)

project(qmmf_system_client)

if (NOT SYSTEM_CLIENT_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
else()
#EXPORT_HEADERS
install(FILES
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_system.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_system_params.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_device.h
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_codec.h
 DESTINATION include/qmmf-sdk)
endif()

add_library(qmmf_system_client SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_system.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_system_client.cc
)

target_include_directories(qmmf_system_client
 PRIVATE ${TOP_DIRECTORY})

install(TARGETS qmmf_system_client DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_system_client log binder utils cutils)

endif()