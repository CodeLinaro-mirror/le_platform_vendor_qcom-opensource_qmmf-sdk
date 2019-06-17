cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_OVERLAY_TEST)

project(qmmf_overlay_test)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_SKIA=0 -DUSE_CAIRO=1")

if (NOT OVERLAY_TEST_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_executable(qmmf_overlay_test ${exclude} qmmf_overlay_test.cc)
add_dependencies(qmmf_overlay_test qmmf_overlay)

target_include_directories(qmmf_overlay_test
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_overlay_test
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

install(TARGETS qmmf_overlay_test DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_overlay_test log cutils utils qmmf_overlay)

endif()