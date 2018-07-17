# Build libqmmf_res_neon.so

lib_LTLIBRARIES = libqmmf_common_resizer_neon.la

libqmmf_common_resizer_neon_la_SOURCES = res_neon.cc
libqmmf_common_resizer_neon_la_SOURCES += qmmf_resizer_neon.cc

libqmmf_common_resizer_neon_la_CPPFLAGS = -I$(top_srcdir)/include
libqmmf_common_resizer_neon_la_CPPFLAGS += -I$(top_srcdir)/

libqmmf_common_resizer_neon_la_LDFLAGS = -lutils -lpthread -llog -lcutils
