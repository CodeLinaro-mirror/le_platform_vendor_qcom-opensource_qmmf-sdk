# Build player test application binary
bin_PROGRAMS := player_gtest

player_gtest_SOURCES  := qmmf_player_gtest.cpp
player_gtest_SOURCES  += qmmf_player_parser.cpp
player_gtest_CPPFLAGS := -I$(top_srcdir)/include
player_gtest_CPPFLAGS += -I$(top_srcdir)/
player_gtest_CPPFLAGS += -std=c++11 -std=gnu++0x -std=c++0x -std=gnu++11
player_gtest_LDFLAGS  := -lutils -lcutils -llog -lpthread 
player_gtest_LDADD    := ../../src/client/libqmmf_player_client.la

