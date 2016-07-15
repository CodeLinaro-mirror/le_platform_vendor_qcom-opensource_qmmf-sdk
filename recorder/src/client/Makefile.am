source_files := qmmf_recorder.cpp
source_files += qmmf_recorder_client.cpp

# Build qmmf recorder client library
# libqmmf_recorder_client.so
lib_LTLIBRARIES := libqmmf_recorder_client.la

libqmmf_recorder_client_la_CPPFLAGS := -I$(top_srcdir)/recorder/inc
libqmmf_recorder_client_la_CPPFLAGS += -I$(top_srcdir)/recorder/src/service
libqmmf_recorder_client_la_CPPFLAGS += -I$(top_srcdir)/common/cameraadaptor
libqmmf_recorder_client_la_CPPFLAGS += -DHAVE_PTHREADS
libqmmf_recorder_client_la_CPPFLAGS += -DLINUX_ENABLED=1
libqmmf_recorder_client_la_CPPFLAGS += -std=c++11 -std=gnu++0x -std=c++0x -std=gnu++11
libqmmf_recorder_client_la_LDFLAGS  := -llog -lbinder -lutils -lcutils

libqmmf_recorder_client_la_SOURCES := $(source_files)

Export recorder client headers:
recorderincdir=$(includedir)/recorder/
recorderinc_HEADERS := ../../inc/qmmf_recorder.h
recorderinc_HEADERS += ../../inc/qmmf_recorder_params.h
