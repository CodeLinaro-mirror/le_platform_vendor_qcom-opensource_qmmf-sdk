cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_SYSTEM_TEST)

project(qmmf_system_test)

if (NOT SYSTEM_SAMPLES_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_executable(qmmf_system_test ${exclude} qmmf_system_test.cc qmmf_system_test_wav.cc)
add_dependencies(qmmf_system_test qmmf_system_client)

target_include_directories(qmmf_system_test
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_system_test
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

install(TARGETS qmmf_system_test DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_system_test log cutils utils pthread qmmf_system_client)

endif()