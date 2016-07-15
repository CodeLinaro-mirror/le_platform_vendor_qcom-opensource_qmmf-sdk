source_files := qmmf_audio_endpoint.cpp
source_files += qmmf_audio_endpoint_client.cpp

# Build qmmf audio endpoint client library
lib_LTLIBRARIES := libqmmf_audio_endpoint_client.la

libqmmf_audio_endpoint_client_la_CPPFLAGS := -I$(top_srcdir)/common/audio/inc
libqmmf_audio_endpoint_client_la_CPPFLAGS += -I$(top_srcdir)/common/audio/src/service
libqmmf_audio_endpoint_client_la_CPPFLAGS += -DHAVE_PTHREADS
libqmmf_audio_endpoint_client_la_CPPFLAGS += -DLINUX_ENABLED=1
libqmmf_audio_endpoint_client_la_CPPFLAGS += -std=c++11 -std=gnu++0x -std=c++0x -std=gnu++11
libqmmf_audio_endpoint_client_la_LDFLAGS  := -llog -lbinder -lutils -lcutils

libqmmf_audio_endpoint_client_la_SOURCES := $(source_files)

Export audio endpoint client headers:
audio_endpointincdir=$(includedir)/audio/
audio_endpointinc_HEADERS := ../../inc/qmmf_audio_endpoint.h
audio_endpointinc_HEADERS += ../../inc/qmmf_audio_params.h

#pkgconfigdir := $(libdir)/pkgconfig
#pkgconfig_DATA := libutils.pc
#EXTRA_DIST := $(pkgconfig_DATA)
