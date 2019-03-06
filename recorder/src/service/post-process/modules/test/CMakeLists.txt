cmake_minimum_required(VERSION 3.1)

project(qmmf_postproc_test)

if (NOT RECORDER_SERVICE_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_postproc_test SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_postproc_test.cc
)

target_include_directories(qmmf_postproc_test
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_postproc_test
 PRIVATE ${TOP_DIRECTORY}/recorder/src/service/post-process)

target_include_directories(qmmf_postproc_test
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_postproc_test DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_postproc_test log utils cutils dl binder pthread)