cmake_minimum_required(VERSION 3.1)

project(qmmf_exif_generator)

if (NOT EXIF_GENERATOR_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
endif()

add_library(qmmf_exif_generator SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_exif_generator.cc
)

target_include_directories(qmmf_exif_generator
 PRIVATE ${TOP_DIRECTORY})

install(TARGETS qmmf_exif_generator DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_exif_generator log binder utils cutils dl pthread camera_metadata)
if(NOT CAMERA_CLIENT_DISABLED)
target_link_libraries(qmmf_exif_generator camera_client)
endif()
