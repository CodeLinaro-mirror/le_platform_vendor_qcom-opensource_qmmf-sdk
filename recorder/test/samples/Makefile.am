# Build recorder test application binary
bin_PROGRAMS := recorder_test

recorder_test_SOURCES  := qmmf_recorder_test.cpp
recorder_test_SOURCES  += qmmf_recorder_test_wav.cc
recorder_test_CPPFLAGS := -I$(top_srcdir)/include
recorder_test_CPPFLAGS += -I$(top_srcdir)/
recorder_test_CPPFLAGS += -std=c++11 -std=gnu++0x -std=c++0x -std=gnu++11
recorder_test_LDFLAGS  := -lutils -lcutils -llog $(LIBGTEST)
recorder_test_LDADD    := ../../src/client/libqmmf_recorder_client.la

