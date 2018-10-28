# Bin Program for Screen nail App
bin_PROGRAMS = screen_nail
screen_nail_SOURCES = qmmf_screen_nail_app.cc
screen_nail_CPPFLAGS = $(CFLAGS)
screen_nail_CPPFLAGS += -I$(top_srcdir)/include
screen_nail_CPPFLAGS += -I$(top_srcdir)/
screen_nail_LDFLAGS  = -lutils -lcutils -llog -lfastcvopt
screen_nail_LDADD    = ../demuxer/libdemuxer.la
screen_nail_LDADD    += ../../../avcodec/libav_codec.la