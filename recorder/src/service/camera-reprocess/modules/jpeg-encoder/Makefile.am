# Build libqmmf_camera_jpeg
lib_LTLIBRARIES = libqmmf_camera_jpeg.la

libqmmf_camera_jpeg_la_SOURCES = qmmf_camera_jpeg_core.cc
libqmmf_camera_jpeg_la_SOURCES += qmmf_camera_jpeg.cc

libqmmf_camera_jpeg_la_CPPFLAGS = -I$(top_srcdir)/include
libqmmf_camera_jpeg_la_CPPFLAGS += -I$(top_srcdir)/
libqmmf_camera_jpeg_la_CPPFLAGS += -I$(top_srcdir)/jpegencoder
libqmmf_camera_jpeg_la_CPPFLAGS += -I$(LIBGRALLOC)
libqmmf_camera_jpeg_la_CPPFLAGS += -I$(CAMERASTACKCOMMON_CFLAGS)
libqmmf_camera_jpeg_la_CPPFLAGS += -I$(CAMERAINTERFACE_CFLAGS)
libqmmf_camera_jpeg_la_CPPFLAGS += -I$(EXIF_CFLAGS)
libqmmf_camera_jpeg_la_CPPFLAGS += -I$(OMXCORE_CFLAGS)
libqmmf_camera_jpeg_la_CPPFLAGS += -I$(OPENMAX_CFLAGS)
libqmmf_camera_jpeg_la_LDFLAGS  = -lutils -lpthread -ldl -llog -lbinder -lcutils
