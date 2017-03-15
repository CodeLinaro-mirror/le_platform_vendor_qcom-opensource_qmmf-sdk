# Build libqmmf_camera_haze_buster
lib_LTLIBRARIES = libqmmf_camera_haze_buster.la

libqmmf_camera_haze_buster_la_SOURCES = qmmf_camera_haze_buster.cc

libqmmf_camera_haze_buster_la_CPPFLAGS  = -I$(top_srcdir)/include
libqmmf_camera_haze_buster_la_CPPFLAGS += -I$(top_srcdir)/
libqmmf_camera_haze_buster_la_CPPFLAGS += -I$(LIBGRALLOC)

libqmmf_camera_haze_buster_la_LDFLAGS  = -lutils -lpthread -ldl -llog -lbinder -lcutils
