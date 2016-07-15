# Build recorder test application binary
bin_PROGRAMS := recorder_gtest

recorder_gtest_SOURCES  := qmmf_recorder_gtest.cpp
recorder_gtest_CPPFLAGS := -I$(top_srcdir)/recorder/inc
recorder_gtest_CPPFLAGS += -std=c++11 -std=gnu++0x -std=c++0x -std=gnu++11
recorder_gtest_LDFLAGS  := -lutils -lcutils -llog -lgtest -lgtest_main
recorder_gtest_LDADD    := ../../src/client/libqmmf_recorder_client.la
