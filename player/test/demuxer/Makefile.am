# Build libdemuxer.so
lib_LTLIBRARIES = libdemuxer.la

libdemuxer_la_SOURCES = qmmf_demuxer_sourceport.cc
libdemuxer_la_SOURCES += qmmf_demuxer_intf.cc

libdemuxer_la_CPPFLAGS = $(CFLAGS)
libdemuxer_la_CPPFLAGS += -I$(top_srcdir)/include
libdemuxer_la_CPPFLAGS += -I$(top_srcdir)/
libdemuxer_la_CPPFLAGS += -I${WORKSPACE}/video/lib/mm-parser/Api/inc
libdemuxer_la_CPPFLAGS += -I${WORKSPACE}/video/lib/mm-osal/inc
libdemuxer_la_CPPFLAGS += -I$(MMCORE_CFLAGS)
libdemuxer_la_LDFLAGS  = -lutils -lcutils -lpthread -ldl -llog -lmmosal -lmmparser_lite
