cmake_minimum_required(VERSION 3.1)

project(qmmf_common_resizer_c2d)

if (NOT RESIZER_C2D_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_common_resizer_c2d SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_resizer_c2d.cc
)

target_include_directories(qmmf_common_resizer_c2d
 PRIVATE ${TOP_DIRECTORY})

target_include_directories(qmmf_common_resizer_c2d
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

target_include_directories(qmmf_common_resizer_c2d
 PRIVATE ${TOP_DIRECTORY}/common/memory)

install(TARGETS qmmf_common_resizer_c2d DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_common_resizer_c2d log utils cutils pthread dl C2D2 jsoncpp)