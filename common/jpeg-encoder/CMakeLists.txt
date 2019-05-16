cmake_minimum_required(VERSION 3.1)

if (NOT DISABLE_PP_JPEG)

project(qmmf_common_jpeg_encoder)

if (NOT JPEG_ENCODER_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_common_jpeg_encoder SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_jpeg_encoder.cc
)
add_dependencies(qmmf_common_jpeg_encoder qmmf_utils)

target_include_directories(qmmf_common_jpeg_encoder
 PRIVATE ${TOP_DIRECTORY})
target_include_directories(qmmf_common_jpeg_encoder
 PRIVATE ${TOP_DIRECTORY}/common)
target_include_directories(qmmf_common_jpeg_encoder
 PRIVATE $<BUILD_INTERFACE:${KERNEL_INCDIR}/usr/include>)

install(TARGETS qmmf_common_jpeg_encoder DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_common_jpeg_encoder qmmf_utils utils cutils pthread dl log hardware camera_metadata)

endif()