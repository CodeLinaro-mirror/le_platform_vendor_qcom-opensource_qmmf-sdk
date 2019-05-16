cmake_minimum_required(VERSION 3.1)

project(qmmf_postproc_algo)

if (NOT RECORDER_SERVICE_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_postproc_algo SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_postproc_algo.cc
)

target_include_directories(qmmf_postproc_algo
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_postproc_algo
 PRIVATE ${TOP_DIRECTORY}/recorder/src/service/post-process)

target_include_directories(qmmf_postproc_algo
 PRIVATE ${TOP_DIRECTORY}/common/memory)

target_include_directories(qmmf_postproc_algo
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_include_directories(qmmf_postproc_algo
 PRIVATE ${PKG_CONFIG_SYSROOT_DIR}/usr/include/ion_headers)

install(TARGETS qmmf_postproc_algo DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_postproc_algo jsoncpp log utils cutils dl binder pthread)

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_link_libraries(qmmf_postproc_algo ion)