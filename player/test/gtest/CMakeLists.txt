cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_PLAYER_SERVICE)

project(qmmf_player_gtest)

if (NOT PLAYER_GTEST_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_executable(qmmf_player_gtest ${exclude} qmmf_player_gtest.cc qmmf_player_parser.cc)
add_dependencies(qmmf_player_gtest qmmf_player_client)

target_include_directories(qmmf_player_gtest
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_player_gtest
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_player_gtest
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_player_gtest DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_player_gtest log cutils utils gtest gtest_main pthread qmmf_player_client)

endif()