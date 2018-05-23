# Build libqmmf_common_resizer_c2d
lib_LTLIBRARIES = libqmmf_common_resizer_c2d.la

libqmmf_common_resizer_c2d_la_SOURCES = qmmf_resizer_c2d.cc

libqmmf_common_resizer_c2d_la_CPPFLAGS = -I$(top_srcdir)/include
libqmmf_common_resizer_c2d_la_CPPFLAGS += -I$(top_srcdir)/
libqmmf_common_resizer_c2d_la_CPPFLAGS += -I$(MMCORE_CFLAGS)

libqmmf_common_resizer_c2d_la_LDFLAGS  = -lutils -ldl -llog -lcutils -lC2D2
