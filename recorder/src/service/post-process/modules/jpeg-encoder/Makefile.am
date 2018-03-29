# Build libqmmf_jpeg
lib_LTLIBRARIES = libqmmf_jpeg.la

libqmmf_jpeg_la_SOURCES = qmmf_jpeg.cc

libqmmf_jpeg_la_CPPFLAGS = -I$(top_srcdir)/include
libqmmf_jpeg_la_CPPFLAGS += -I$(top_srcdir)/
libqmmf_jpeg_la_LDFLAGS  = -lutils -ldl -llog -lcutils

libqmmf_jpeg_la_LIBADD   = ../../../../../../common/jpeg-encoder/libqmmf_common_jpeg_encoder.la
