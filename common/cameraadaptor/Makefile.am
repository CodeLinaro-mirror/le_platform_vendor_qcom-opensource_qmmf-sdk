# Build libcamera_adaptor.so
lib_LTLIBRARIES = libcamera_adaptor.la

libcamera_adaptor_la_SOURCES := qmmf_camera3_device_client.cpp
libcamera_adaptor_la_SOURCES += qmmf_camera3_monitor.cpp
libcamera_adaptor_la_SOURCES += qmmf_camera3_request_handler.cpp
libcamera_adaptor_la_SOURCES += qmmf_camera3_prepare_handler.cpp
libcamera_adaptor_la_SOURCES += qmmf_camera3_stream.cpp
libcamera_adaptor_la_SOURCES += qmmf_camera3_thread.cpp
libcamera_adaptor_la_SOURCES += qmmf_camera3_utils.cpp

libcamera_adaptor_la_CPPFLAGS := -std=c++11 -Wall
libcamera_adaptor_la_CPPFLAGS += -I$(top_srcdir)/include
libcamera_adaptor_la_CPPFLAGS += -I$(top_srcdir)/
libcamera_adaptor_la_CPPFLAGS += -I$(LIBGRALLOC)
libcamera_adaptor_la_LDFLAGS  := -lutils -lbinder -lcamera_client
libcamera_adaptor_la_LDFLAGS  += -lcamera_metadata -lpthread -ldl -llog

camera_adaptordir=$(includedir)
camera_adaptor_HEADERS := qmmf_camera3_types.h

# Adaptor gtest app
bin_PROGRAMS := camera_adaptor_gtest

camera_adaptor_gtest_SOURCES  := gtest/qmmf_camera_adaptor_gtest.cpp
camera_adaptor_gtest_CPPFLAGS := $(libcamera_adaptor_la_CPPFLAGS) -I$(top_srcdir) -I$(LIBGRALLOC)
camera_adaptor_gtest_LDADD    := libcamera_adaptor.la -lgtest -lgtest_main
camera_adaptor_gtest_LDFLAGS  := $(libcamera_adaptor_la_LDFLAGS)

# Dual adaptor gtest app
bin_PROGRAMS += camera_dual_adaptor_gtest

camera_dual_adaptor_gtest_SOURCES  := gtest/qmmf_dual_camera_adaptor_gtest.cpp
camera_dual_adaptor_gtest_CPPFLAGS := $(libcamera_adaptor_la_CPPFLAGS) -I$(top_srcdir) -I$(LIBGRALLOC)
camera_dual_adaptor_gtest_LDADD    := libcamera_adaptor.la -lgtest -lgtest_main
camera_dual_adaptor_gtest_LDFLAGS  := $(libcamera_adaptor_la_LDFLAGS)
