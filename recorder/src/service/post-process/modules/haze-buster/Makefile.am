# Build libqmmf_haze_buster
lib_LTLIBRARIES = libqmmf_haze_buster.la

libqmmf_haze_buster_la_SOURCES = qmmf_haze_buster.cc

libqmmf_haze_buster_la_CPPFLAGS  = -I$(top_srcdir)/include
libqmmf_haze_buster_la_CPPFLAGS += -I$(top_srcdir)/
libqmmf_haze_buster_la_CPPFLAGS += -I$(LIBGRALLOC)

libqmmf_haze_buster_la_LDFLAGS  = -lutils -lpthread -ldl -llog -lbinder -lcutils
