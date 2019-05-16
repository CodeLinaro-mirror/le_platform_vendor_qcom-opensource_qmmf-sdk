cmake_minimum_required(VERSION 3.1)

project(qmmf_av_queue)

if (NOT AVQUEUE_ENABLED)
set(exclude EXCLUDE_FROM_ALL)
else()
#EXPORT_HEADERS
install(FILES
 ${TOP_DIRECTORY}/include/qmmf-sdk/qmmf_queue.h
 DESTINATION include/qmmf-sdk)
endif()

add_library(qmmf_av_queue SHARED ${exclude}
${CMAKE_CURRENT_SOURCE_DIR}/qmmf_queue.cc
)

target_include_directories(qmmf_av_queue
 PRIVATE ${TOP_DIRECTORY})

install(TARGETS qmmf_av_queue DESTINATION lib OPTIONAL)
target_link_libraries(qmmf_av_queue log utils cutils)