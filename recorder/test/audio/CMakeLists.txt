cmake_minimum_required(VERSION 3.1)

project(qmmf_recorder_atest)

add_executable(qmmf_recorder_atest
qmmf_recorder_test.cc
qmmf_recorder_test_wav.cc)
add_dependencies(qmmf_recorder_atest qmmf_recorder_client qmmf_utils)

target_include_directories(qmmf_recorder_atest
 PRIVATE ${TOP_DIRECTORY})

install(TARGETS qmmf_recorder_atest DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_recorder_atest log cutils utils qmmf_utils qmmf_recorder_client)
