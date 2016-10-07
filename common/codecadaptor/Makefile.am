# Build libcodec_adaptor.so
lib_LTLIBRARIES = libcodec_adaptor.la

libcodec_adaptor_la_SOURCES := src/qmmf_omx_client.cpp
libcodec_adaptor_la_SOURCES += src/qmmf_avcodec.cpp

libcodec_adaptor_la_CPPFLAGS := -std=c++11 -Wall
libcodec_adaptor_la_CPPFLAGS += -I$(top_srcdir)/include
libcodec_adaptor_la_CPPFLAGS += -I$(top_srcdir)/
libcodec_adaptor_la_CPPFLAGS += -I$(MMCORE_CFLAGS)
libcodec_adaptor_la_LDFLAGS  := -lutils -lcutils -lpthread -ldl -llog

# Codec Adaptor sample test app
bin_PROGRAMS := codec_adaptor_test

codec_adaptor_test_SOURCES  := test/sample/qmmf_codec_adaptor_test.cpp
codec_adaptor_test_CPPFLAGS := $(libcodec_adaptor_la_CPPFLAGS) -I$(top_srcdir)
codec_adaptor_test_LDADD    := libcodec_adaptor.la

# Codec Adaptor gtest app
bin_PROGRAMS += codec_adaptor_gtest

codec_adaptor_gtest_SOURCES  := test/gtest/qmmf_codec_adaptor_gtest.cpp
codec_adaptor_gtest_CPPFLAGS := $(libcodec_adaptor_la_CPPFLAGS) -I$(top_srcdir)
codec_adaptor_gtest_LDFLAGS  := -lgtest -lgtest_main
codec_adaptor_gtest_LDADD    := libcodec_adaptor.la

# Codec Adaptor sample audio secode test app
bin_PROGRAMS += codec_adaptor_audio_decode_test

codec_adaptor_audio_decode_test_SOURCES  := test/sample/qmmf_audio_decode_test.cpp
codec_adaptor_audio_decode_test_CPPFLAGS := $(libcodec_adaptor_la_CPPFLAGS) -I$(top_srcdir)
codec_adaptor_audio_decode_test_LDADD    := libcodec_adaptor.la
