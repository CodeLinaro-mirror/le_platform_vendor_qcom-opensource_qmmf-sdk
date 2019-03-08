# Build qmmf audio service library
lib_LTLIBRARIES = libqmmf_audio_service.la

source_files = qmmf_audio_service.cc
source_files += qmmf_audio_ion.cc
source_files += qmmf_audio_frontend.cc
source_files += qmmf_audio_backend_source.cc
source_files += qmmf_audio_backend_sink.cc

cpp_flags = -I$(top_srcdir)
cpp_flags += -I$(top_srcdir)/include
cpp_flags += -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include/ion_headers

ld_flags = -llog
ld_flags += -lbinder
ld_flags += -lutils
ld_flags += -lcutils
ld_flags += -lqahw
ld_flags += -lion

libqmmf_audio_service_la_SOURCES  = $(source_files)
libqmmf_audio_service_la_CPPFLAGS = $(cpp_flags)
libqmmf_audio_service_la_LDFLAGS  = $(ld_flags)

libqmmf_audio_service_la_LIBADD   = ../client/libqmmf_audio_client.la
