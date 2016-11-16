# Build player test application binary
bin_PROGRAMS := player_test

player_test_SOURCES  := qmmf_player_test.cc
player_test_SOURCES  += qmmf_player_parser.cc
player_test_CPPFLAGS := -I$(top_srcdir)/include
player_test_CPPFLAGS += -I$(top_srcdir)/
player_test_CPPFLAGS += -I${WORKSPACE}/video/lib/mm-parser/Api/inc
player_test_CPPFLAGS += -I${WORKSPACE}/video/lib/mm-osal/inc
player_test_CPPFLAGS += -std=c++11 -std=gnu++0x -std=c++0x -std=gnu++11 $(CFLAGS)
player_test_LDFLAGS  := -lutils -lcutils -llog -lpthread
player_test_LDADD    := ../../src/client/libqmmf_player_client.la
player_test_LDADD    += ../demuxer/libdemuxer.la
