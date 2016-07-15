BUILD_COMPONENTS :=
BUILD_COMPONENTS += common/cameraadaptor
BUILD_COMPONENTS += recorder/src/client
BUILD_COMPONENTS += recorder/src/service
BUILD_COMPONENTS += recorder/test/samples
BUILD_COMPONENTS += recorder/test/gtest
BUILD_COMPONENTS += qmmf-server

SUBDIRS := $(BUILD_COMPONENTS)

pkgconfigdir := $(libdir)/pkgconfig
pkgconfig_DATA := qmmf-sdk.pc
EXTRA_DIST := $(pkgconfig_DATA)
