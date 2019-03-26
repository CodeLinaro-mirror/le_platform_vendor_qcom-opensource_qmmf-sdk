cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_PLAYER_SERVICE)

project(qmmf_player_test)

if (NOT PLAYER_SAMPLES_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_executable(qmmf_player_test ${exclude} qmmf_player_test.cc)
add_dependencies(qmmf_player_test qmmf_demuxer qmmf_player_client qmmf_display_client)

target_include_directories(qmmf_player_test
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_player_test
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_player_test
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_player_test DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_player_test log cutils utils pthread qmmf_demuxer qmmf_player_client qmmf_display_client)

add_executable(qmmf_player_parser_test ${exclude} qmmf_player_parser_test.cc qmmf_player_parser.cc)
add_dependencies(qmmf_player_parser_test qmmf_player_client)

target_include_directories(qmmf_player_parser_test
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_player_parser_test
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_player_parser_test
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_player_parser_test DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_player_parser_test log cutils utils pthread qmmf_player_client)

endif()