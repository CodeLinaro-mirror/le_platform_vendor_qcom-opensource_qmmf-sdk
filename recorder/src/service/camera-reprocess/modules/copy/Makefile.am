# Build libqmmf_camera_copy
lib_LTLIBRARIES = libqmmf_camera_copy.la

libqmmf_camera_copy_la_SOURCES = qmmf_camera_copy.cc

libqmmf_camera_copy_la_CPPFLAGS = -I$(top_srcdir)/include
libqmmf_camera_copy_la_CPPFLAGS += -I$(top_srcdir)/
libqmmf_camera_copy_la_CPPFLAGS += -I$(LIBGRALLOC)
libqmmf_camera_copy_la_LDFLAGS  = -lutils -lpthread -ldl -llog -lbinder -lcutils
