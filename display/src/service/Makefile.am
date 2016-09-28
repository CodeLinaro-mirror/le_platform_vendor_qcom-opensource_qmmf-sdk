source_files := qmmf_display_service.cpp
source_files += ../client/qmmf_display_client.cpp
#source_files += qmmf_display_common.cpp
source_files += qmmf_display_impl.cpp
source_files += qmmf_remote_cb.cpp
source_files += qmmf_display_sdm_buffer_allocator.cpp
source_files += qmmf_display_sdm_debugger.cpp
source_files += qmmf_display_sdm_buffer_sync_handler.cpp


# Build qmmf display service library
# libqmmf_display_service.so
lib_LTLIBRARIES := libqmmf_display_service.la

libqmmf_display_service_la_SOURCES := $(source_files)

libqmmf_display_service_la_CPPFLAGS := -I$(top_srcdir)/include
libqmmf_display_service_la_CPPFLAGS += -I$(top_srcdir)/
libqmmf_display_service_la_CPPFLAGS += -I$(top_srcdir)/display/src/client
libqmmf_display_service_la_CPPFLAGS += -I$(LIBGRALLOC)
libqmmf_display_service_la_CPPFLAGS += -I$(MMCORE_CFLAGS)
libqmmf_display_service_la_CPPFLAGS += -I${WORKSPACE}/display/display-hal/sdm/include/
libqmmf_display_service_la_CPPFLAGS += -DHAVE_PTHREADS
libqmmf_display_service_la_CPPFLAGS += -DLINUX_ENABLED=1
libqmmf_display_service_la_CPPFLAGS += -std=c++11 -std=gnu++0x -std=c++0x -std=gnu++11
libqmmf_display_service_la_CFLAGS   := -std=c++11 -std=gnu++0x -std=c++0x -std=gnu++11
libqmmf_display_service_la_LDFLAGS  := -llog -lbinder -lutils -lcutils
libqmmf_display_service_la_LDFLAGS  += -lsdmcore -lsdmutils -lmemalloc


libqmmf_display_service_la_LIBADD   := ../client/libqmmf_display_client.la
