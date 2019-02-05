# Build qmmf audio client library
lib_LTLIBRARIES = libqmmf_audio_client.la

source_files = qmmf_audio_endpoint.cc
source_files += qmmf_audio_endpoint_client.cc

cpp_flags = -I$(top_srcdir)
cpp_flags += -I$(top_srcdir)/include
cpp_flags += -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include/ion_headers

ld_flags = -llog
ld_flags += -lbinder
ld_flags += -lutils
ld_flags += -lcutils
ld_flags += -lion

libqmmf_audio_client_la_SOURCES  = $(source_files)
libqmmf_audio_client_la_CPPFLAGS = $(cpp_flags)
libqmmf_audio_client_la_LDFLAGS  = $(ld_flags)
