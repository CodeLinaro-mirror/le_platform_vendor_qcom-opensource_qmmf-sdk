source_files = qmmf_camera_hal.cc

# Build qmmf recorder service library
# qmmf_camera_hal.so
lib_LTLIBRARIES = libqmmf_camera_hal.la

libqmmf_camera_hal_la_SOURCES = $(source_files)

libqmmf_camera_hal_la_CPPFLAGS = -I$(top_srcdir)/include
libqmmf_camera_hal_la_CPPFLAGS += -I$(top_srcdir)/
libqmmf_camera_hal_la_CPPFLAGS += -I$(LIBGRALLOC)
libqmmf_camera_hal_la_CPPFLAGS += -I$(MMCORE_CFLAGS)
libqmmf_camera_hal_la_CPPFLAGS += -I$(CAMERAHAL_CFLAGS)
libqmmf_camera_hal_la_LDFLAGS  = -llog -lbinder -lutils -lcutils
libqmmf_camera_hal_la_LDFLAGS  += -lcamera_client -lcamera_metadata

#libqmmf_camera_hal_la_LIBADD

