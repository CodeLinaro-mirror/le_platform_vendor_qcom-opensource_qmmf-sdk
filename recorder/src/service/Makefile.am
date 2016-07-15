source_files := qmmf_recorder_service.cpp
source_files += ../client/qmmf_recorder_client.cpp
source_files += qmmf_recorder_common.cpp
source_files += qmmf_recorder_impl.cpp
source_files += qmmf_remote_cb.cpp
source_files += qmmf_camera_source.cpp
source_files += qmmf_camera_context.cpp
source_files += qmmf_encoder_core.cpp

# Build qmmf recorder service library
# libqmmf_recorder_service.so
lib_LTLIBRARIES := libqmmf_recorder_service.la

libqmmf_recorder_service_la_SOURCES := $(source_files)

libqmmf_recorder_service_la_CPPFLAGS := -I$(top_srcdir)/recorder/inc
libqmmf_recorder_service_la_CPPFLAGS += -I$(top_srcdir)/recorder/src/client
libqmmf_recorder_service_la_CPPFLAGS += -I$(top_srcdir)/common/cameraadaptor
libqmmf_recorder_service_la_CPPFLAGS += -I$(LIBGRALLOC)
libqmmf_recorder_service_la_CPPFLAGS += -DHAVE_PTHREADS
libqmmf_recorder_service_la_CPPFLAGS += -DLINUX_ENABLED=1
libqmmf_recorder_service_la_CPPFLAGS += -std=c++11 -std=gnu++0x -std=c++0x -std=gnu++11
libqmmf_recorder_service_la_CFLAGS   := -std=c++11 -std=gnu++0x -std=c++0x -std=gnu++11
libqmmf_recorder_service_la_LDFLAGS  := -llog -lbinder -lutils -lcutils -lcamera_metadata

libqmmf_recorder_service_la_LIBADD   := ../client/libqmmf_recorder_client.la
libqmmf_recorder_service_la_LIBADD   += ../../../common/cameraadaptor/libcamera_adaptor.la
