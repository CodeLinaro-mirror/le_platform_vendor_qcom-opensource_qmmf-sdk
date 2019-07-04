cmake_minimum_required(VERSION 3.1)
if (NOT DISABLE_QMMF_PLUGIN)

if (NOT PLUGINS_ENABLED)
  set(exclude EXCLUDE_FROM_ALL)
endif()

# Build qmmf test outplace algorithm library
add_library(qmmf_test_algo_outplace SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/sample-plugins/qmmf_test_algo_outplace.cc
)

install(TARGETS qmmf_test_algo_outplace DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_test_algo_outplace log utils cutils)

# Build qmmf test inplace algorithm library
add_library(qmmf_test_algo_inplace SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/sample-plugins/qmmf_test_algo_inplace.cc
)

install(TARGETS qmmf_test_algo_inplace DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_test_algo_inplace log utils cutils)

# Build qmmf test outplace algorithm with history library
add_library(qmmf_test_algo_outplace_history SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/sample-plugins/qmmf_test_algo_outplace_history.cc
)

install(TARGETS qmmf_test_algo_outplace_history DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_test_algo_outplace_history log utils cutils)

# Build qmmf test inplace algorithm with history library
add_library(qmmf_test_algo_inplace_history SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/sample-plugins/qmmf_test_algo_inplace_history.cc
)

install(TARGETS qmmf_test_algo_inplace_history DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_test_algo_inplace_history log utils cutils)

# Build qmmf test resizer library
add_library(qmmf_test_algo_resizer SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/sample-plugins/qmmf_test_algo_resizer.cc
)

install(TARGETS qmmf_test_algo_resizer DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_test_algo_resizer log utils cutils)

# Build qmmf algorithm interface gtest application binary
add_executable(qmmf_algo_interface_gtest ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/gtest/qmmf_algo_interface_gtest.cc
${CMAKE_CURRENT_SOURCE_DIR}/gtest/buffer_handler.cc
${CMAKE_CURRENT_SOURCE_DIR}/gtest/qmmf_algo_gtest_configuration_buffer.cc
${CMAKE_CURRENT_SOURCE_DIR}/gtest/qmmf_algo_gtest_test_content.cc
${CMAKE_CURRENT_SOURCE_DIR}/gtest/qmmf_algo_gtest_test_suite.cc
${CMAKE_CURRENT_SOURCE_DIR}/gtest/qmmf_algo_gtest_configuration.cc
${CMAKE_CURRENT_SOURCE_DIR}/gtest/heap_buffer.cc
${CMAKE_CURRENT_SOURCE_DIR}/gtest/heap_tracker.cc
)

target_include_directories(qmmf_algo_interface_gtest
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_algo_interface_gtest
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

# TODO remove this hack when camx issue with propagating c and cpp flags is solved
target_include_directories(qmmf_algo_interface_gtest
 PRIVATE ${PKG_CONFIG_SYSROOT_DIR}/usr/include/ion_headers)

install(TARGETS qmmf_algo_interface_gtest DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_algo_interface_gtest log utils cutils gtest gtest_main dl jsoncpp pthread)

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_link_libraries(qmmf_algo_interface_gtest ion)

# Build qmmf algorithm interface gtest application binary
add_executable(qmmf_utils_gtest ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/gtest/qmmf_utils_gtest.cc
)

target_include_directories(qmmf_utils_gtest
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_utils_gtest
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

 # TODO remove this hack when camx issue with propagating c and cpp flags is solved
target_include_directories(qmmf_utils_gtest
 PRIVATE ${PKG_CONFIG_SYSROOT_DIR}/usr/include/ion_headers)

install(TARGETS qmmf_utils_gtest DESTINATION bin OPTIONAL)
target_link_libraries(qmmf_utils_gtest log utils cutils gtest gtest_main dl)

# TODO remove this hack when camx issue with propagating c and cpp glags is solved
target_link_libraries(qmmf_utils_gtest ion)

if (PLUGINS_ENABLED)
file(GLOB_RECURSE JSON_FILES ${CMAKE_CURRENT_LIST_DIR}/test_cases/*.json)
install(FILES ${JSON_FILES} DESTINATION ${QMMF_DATA})
endif()

endif()