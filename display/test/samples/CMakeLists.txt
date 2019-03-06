cmake_minimum_required(VERSION 3.1)

if (NOT DISABLE_DISPLAY)

project(qmmf_display_test)

if (NOT DISPLAY_SAMPLES_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_executable(qmmf_display_test ${exclude} qmmf_display_test.cc)
add_dependencies(qmmf_display_test qmmf_display_client)

target_include_directories(qmmf_display_test
 PRIVATE ${TOP_DIRECTORY})

install(TARGETS qmmf_display_test DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_display_test log cutils utils qmmf_display_client)
endif()