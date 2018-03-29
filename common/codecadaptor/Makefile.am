# Build libcodec_adaptor.so
lib_LTLIBRARIES = libcodec_adaptor.la

libcodec_adaptor_la_SOURCES = src/qmmf_omx_client.cc
libcodec_adaptor_la_SOURCES += src/qmmf_avcodec.cc
libcodec_adaptor_la_SOURCES += src/qmmf_jpeg_encode.cc

libcodec_adaptor_la_CPPFLAGS = -I$(top_srcdir)/include
libcodec_adaptor_la_CPPFLAGS += -I$(top_srcdir)/
libcodec_adaptor_la_CPPFLAGS += -I$(MMCORE_CFLAGS)
libcodec_adaptor_la_CPPFLAGS += -I$(OMXCORE_CFLAGS)
libcodec_adaptor_la_CPPFLAGS += -I$(OPENMAX_CFLAGS)
libcodec_adaptor_la_LDFLAGS  = -lutils -lcutils -lpthread -ldl -llog  -lhardware -lcamera_metadata

libcodec_adaptor_la_LIBADD   = ../jpeg-encoder/libqmmf_common_jpeg_encoder.la