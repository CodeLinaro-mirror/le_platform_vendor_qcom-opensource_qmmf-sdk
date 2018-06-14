# Build libqmmf_common_resizer_fastcv
lib_LTLIBRARIES = libqmmf_common_resizer_fastcv.la

libqmmf_common_resizer_fastcv_la_SOURCES = qmmf_resizer_fastCV.cc

libqmmf_common_resizer_fastcv_la_CPPFLAGS = -I$(top_srcdir)/include
libqmmf_common_resizer_fastcv_la_CPPFLAGS += -I$(top_srcdir)/
libqmmf_common_resizer_fastcv_la_CPPFLAGS += -I$(MMCORE_CFLAGS)

libqmmf_common_resizer_fastcv_la_LDFLAGS  = -lutils -ldl -llog -lcutils -lfastcvopt
