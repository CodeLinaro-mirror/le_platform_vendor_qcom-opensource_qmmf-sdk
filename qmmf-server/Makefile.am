# Build qmmf-server binary
bin_PROGRAMS := qmmf-server

qmmf_server_SOURCES  := qmmf_server_main.cpp

qmmf_server_CPPFLAGS := -I$(top_srcdir)/recorder/inc
qmmf_server_CPPFLAGS += -I$(top_srcdir)/recorder/src/client
qmmf_server_CPPFLAGS += -I$(top_srcdir)/common/cameraadaptor
qmmf_server_CPPFLAGS += -I$(top_srcdir)/recorder/src/service
qmmf_server_CPPFLAGS += -I$(LIBGRALLOC)
qmmf_server_CPPFLAGS += -std=c++11 -std=gnu++0x -std=c++0x -std=gnu++11

qmmf_server_LDFLAGS  := -lutils -lcutils -llog -lbinder
qmmf_server_LDADD    := ../recorder/src/service/libqmmf_recorder_service.la
