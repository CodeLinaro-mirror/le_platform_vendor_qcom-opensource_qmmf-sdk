# Build display test application binary
bin_PROGRAMS := display_gtest

display_gtest_SOURCES  := qmmf_display_gtest.cc
display_gtest_SOURCES  += qmmf_display_buffer_allocator.cc
display_gtest_CPPFLAGS := -I$(top_srcdir)/include
display_gtest_CPPFLAGS += -I$(top_srcdir)/
display_gtest_CPPFLAGS += -I$(LIBGRALLOC)
display_gtest_CPPFLAGS += -I${WORKSPACE}/display/display-hal/sdm/include/
display_gtest_CPPFLAGS += -std=c++11 -std=gnu++0x -std=c++0x -std=gnu++11
display_gtest_LDFLAGS  := -lutils -lcutils -llog -lgtest -lgtest_main -lmemalloc
display_gtest_LDADD    := ../../src/client/libqmmf_display_client.la
