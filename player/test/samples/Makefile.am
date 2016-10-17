# Build player test application binary
bin_PROGRAMS := player_test

player_test_SOURCES  := qmmf_player_test.cpp
player_test_SOURCES  += qmmf_player_parser.cpp
player_test_CPPFLAGS := -I$(top_srcdir)/include
player_test_CPPFLAGS += -I$(top_srcdir)/
player_test_CPPFLAGS += -std=c++11 -std=gnu++0x -std=c++0x -std=gnu++11
player_test_LDFLAGS  := -lutils -lcutils -llog -lpthread
player_test_LDADD    := ../../src/client/libqmmf_player_client.la

