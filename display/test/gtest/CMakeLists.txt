cmake_minimum_required(VERSION 3.1)

if (NOT DISABLE_DISPLAY)

project(qmmf_display_gtest)

if (NOT DISPLAY_GTEST_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_executable(qmmf_display_gtest ${exclude} qmmf_display_gtest.cc qmmf_display_buffer_allocator.cc)
add_dependencies(qmmf_display_gtest qmmf_display_client)

target_include_directories(qmmf_display_gtest
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_display_gtest
 PRIVATE ${SYSROOT_INCDIR}/qcom/display)

target_include_directories(qmmf_display_gtest
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

install(TARGETS qmmf_display_gtest DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_display_gtest log cutils utils gtest gtest_main memalloc pthread qmmf_display_client)

endif()