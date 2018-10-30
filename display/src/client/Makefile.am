source_files = qmmf_display.cc

if !ENABLE_WESTON_CLIENT
source_files += qmmf_display_client.cc
else
source_files += qmmf_display_weston_client.cc
endif

# Build qmmf display client library
# libqmmf_display_client.so
lib_LTLIBRARIES = libqmmf_display_client.la

libqmmf_display_client_la_CPPFLAGS = -I$(top_srcdir)/include
libqmmf_display_client_la_CPPFLAGS += -I$(top_srcdir)/
libqmmf_display_client_la_CPPFLAGS += -I$(top_srcdir)/common/memory
libqmmf_display_client_la_CPPFLAGS += -I$(MMCORE_CFLAGS)
libqmmf_display_client_la_LDFLAGS  = -llog -lbinder -lutils -lcutils

if TARGET_USES_GBM
libqmmf_display_client_la_LDFLAGS += -lgbm
endif

if ENABLE_WESTON_CLIENT
nodist_libqmmf_display_client_la_SOURCES = \
    gbm-buffer-backend-client-protocol.h

BUILT_SOURCES = $(nodist_libqmmf_display_client_la_SOURCES)

libqmmf_display_client_la_LDFLAGS  += -lwayland-client -lgbm-buffer-backend-protocol

WESTON_PROTOCOL_PATH=$(WORKSPACE)/display/weston/protocol
gbm-buffer-backend-client-protocol.h : $(WESTON_PROTOCOL_PATH)/gbm-buffer-backend.xml
	$(wayland_scanner) client-header < $(WESTON_PROTOCOL_PATH)/gbm-buffer-backend.xml > $(srcdir)/gbm-buffer-backend-client-protocol.h
endif

libqmmf_display_client_la_SOURCES = $(source_files)

Export qmmf-sdk headers:
sdkincdir=$(includedir)/qmmf-sdk
sdkinc_HEADERS = $(top_srcdir)/include/qmmf-sdk/qmmf_recorder.h
sdkinc_HEADERS += $(top_srcdir)/include/qmmf-sdk/qmmf_recorder_params.h
sdkinc_HEADERS += $(top_srcdir)/include/qmmf-sdk/qmmf_display.h
sdkinc_HEADERS += $(top_srcdir)/include/qmmf-sdk/qmmf_display_params.h
sdkinc_HEADERS += $(top_srcdir)/include/qmmf-sdk/qmmf_system.h
sdkinc_HEADERS += $(top_srcdir)/include/qmmf-sdk/qmmf_system_params.h
sdkinc_HEADERS += $(top_srcdir)/include/qmmf-sdk/qmmf_player.h
sdkinc_HEADERS += $(top_srcdir)/include/qmmf-sdk/qmmf_player_params.h
sdkinc_HEADERS += $(top_srcdir)/include/qmmf-sdk/qmmf_device.h
sdkinc_HEADERS += $(top_srcdir)/include/qmmf-sdk/qmmf_codec.h
