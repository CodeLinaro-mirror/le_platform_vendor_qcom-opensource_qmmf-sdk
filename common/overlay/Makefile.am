# Build liboverlay.so
lib_LTLIBRARIES = liboverlay.la

liboverlay_la_SOURCES := qmmf_overlay.cpp

liboverlay_la_CPPFLAGS := -std=c++11 -Wall
liboverlay_la_CPPFLAGS += -I$(top_srcdir)/include
liboverlay_la_CPPFLAGS += -I$(top_srcdir)/
liboverlay_la_CPPFLAGS += -DUSE_SKIA=0
liboverlay_la_LDFLAGS  := -lutils -lcutils -lpthread -ldl -llog -lC2D2

# Codec Adaptor sample test app
bin_PROGRAMS := overlay_test

overlay_test_SOURCES  := test/qmmf_overlay_test.cpp
overlay_test_CPPFLAGS := $(liboverlay_la_CPPFLAGS) -I$(top_srcdir)
overlay_test_LDADD    := liboverlay.la

root_etcdir   := "/etc"
root_etc_DATA := test/raw_image/overlay_test.rgba
