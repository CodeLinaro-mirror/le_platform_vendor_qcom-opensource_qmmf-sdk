cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_AUDIO_TEST)

project(qmmf_audio_test)

if (NOT AUDIO_TEST_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_executable(qmmf_audio_test ${exclude} qmmf_audio_test.cc qmmf_audio_test_ion.cc qmmf_audio_test_wav.cc)
add_dependencies(qmmf_audio_test qmmf_audio_client)

target_include_directories(qmmf_audio_test
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_audio_test
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_audio_test
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_audio_test DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_audio_test log cutils utils pthread qmmf_audio_client)

endif()