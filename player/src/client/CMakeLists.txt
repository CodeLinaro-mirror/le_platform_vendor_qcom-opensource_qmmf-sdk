cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_PLAYER_SERVICE)

project(qmmf_player_client)

if (NOT PLAYER_CLIENT_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
else()
  #EXPORT_HEADERS
  install(FILES
    ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_display.h
    ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_display_params.h
    ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_system.h
    ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_system_params.h
    ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_player.h
    ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_player_params.h
    DESTINATION include/qmmf-sdk)
endif()

add_library(qmmf_player_client SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_player.cc
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_player_client.cc
)

target_include_directories(qmmf_player_client
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_player_client
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_player_client
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_player_client DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_player_client log binder utils cutils)

endif()