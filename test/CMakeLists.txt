project(qmmf_test)

add_executable(qmmf_test ${exclude}
qmmf_test.cpp
)
target_link_libraries(qmmf_test qmmf_recorder_client)
target_link_libraries(qmmf_test gbm)
target_link_libraries(qmmf_test cutils)
target_link_libraries(qmmf_test dl)
include_directories ("/usr/include")

install(TARGETS qmmf_test DESTINATION bin OPTIONAL)

