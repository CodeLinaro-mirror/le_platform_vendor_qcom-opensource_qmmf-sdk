cmake_minimum_required(VERSION 3.1)

project(qmmf_recorder_test)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_SKIA=0 -DUSE_CAIRO=1")

if (NOT RECORDER_SAMPLES_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_executable(qmmf_recorder_test ${exclude}
qmmf_recorder_test.cc
qmmf_recorder_test_wav.cc
qmmf_recorder_test_aac.cc
qmmf_recorder_test_amr.cc
qmmf_recorder_test_mpegh.cc)
add_dependencies(qmmf_recorder_test qmmf_recorder_client qmmf_utils)
if (NOT DISABLE_RECORDER_TEST_DISPLAY)
add_dependencies(qmmf_recorder_test qmmf_display_client)
endif()

target_include_directories(qmmf_recorder_test
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_recorder_test
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_recorder_test DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_recorder_test log cutils utils cairo camera_metadata camera_client gtest gtest_main qmmf_utils qmmf_recorder_client)
if (NOT DISABLE_RECORDER_TEST_DISPLAY)
target_link_libraries(qmmf_recorder_test qmmf_display_client)
endif()