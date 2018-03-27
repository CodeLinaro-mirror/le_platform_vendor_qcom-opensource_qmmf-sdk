# Build libqmmf_common_jpeg_encoder
lib_LTLIBRARIES = libqmmf_common_jpeg_encoder.la

libqmmf_common_jpeg_encoder_la_SOURCES = qmmf_jpeg_encoder.cc

libqmmf_common_jpeg_encoder_la_CPPFLAGS = -I$(top_srcdir)/include
libqmmf_common_jpeg_encoder_la_CPPFLAGS += -I$(top_srcdir)/
libqmmf_common_jpeg_encoder_la_CPPFLAGS += -I$(top_srcdir)/jpegencoder
libqmmf_common_jpeg_encoder_la_CPPFLAGS += -I$(MMCORE_CFLAGS)
libqmmf_common_jpeg_encoder_la_CPPFLAGS += -I$(CAMERAHAL_CFLAGS)
libqmmf_common_jpeg_encoder_la_CPPFLAGS += -I$(CAMERASTACKCOMMON_CFLAGS)
libqmmf_common_jpeg_encoder_la_CPPFLAGS += -I$(CAMERAINTERFACE_CFLAGS)
libqmmf_common_jpeg_encoder_la_CPPFLAGS += -I$(EXIF_CFLAGS)
libqmmf_common_jpeg_encoder_la_CPPFLAGS += -I$(OMXCORE_CFLAGS)
libqmmf_common_jpeg_encoder_la_CPPFLAGS += -I$(OPENMAX_CFLAGS)

libqmmf_common_jpeg_encoder_la_LDFLAGS  = -lutils -lpthread -ldl -llog -lbinder -lcutils
