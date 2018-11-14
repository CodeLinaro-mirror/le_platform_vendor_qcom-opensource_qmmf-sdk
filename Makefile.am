BUILD_COMPONENTS =
BUILD_COMPONENTS += common/utils
BUILD_COMPONENTS += common/memory
BUILD_COMPONENTS += common/cameraadaptor
BUILD_COMPONENTS += common/overlay
BUILD_COMPONENTS += common/audio/src/client
BUILD_COMPONENTS += common/audio/src/service
if !DISABLE_AUDIO_TEST
BUILD_COMPONENTS += common/audio/test/samples
endif
BUILD_COMPONENTS += common/resizer-c2d
if !DISABLE_PP_JPEG
BUILD_COMPONENTS += common/jpeg-encoder
endif
BUILD_COMPONENTS += common/codecadaptor
BUILD_COMPONENTS += common/avqueue
BUILD_COMPONENTS += common/exif-generator
BUILD_COMPONENTS += common/resizer-neon
BUILD_COMPONENTS += common/resizer-fastCV
if !DISABLE_QMMF_PLUGIN
BUILD_COMPONENTS += plugins
endif
BUILD_COMPONENTS += avcodec
if !DISABLE_DISPLAY_CLIENT
BUILD_COMPONENTS += display/src/client
endif
BUILD_COMPONENTS += recorder/src/service/post-process/modules/algo
BUILD_COMPONENTS += recorder/src/service/post-process/modules/camera-hal-reproc
if !DISABLE_PP_JPEG
BUILD_COMPONENTS += recorder/src/service/post-process/modules/jpeg-encoder
endif
BUILD_COMPONENTS += recorder/src/service/post-process/modules/test
BUILD_COMPONENTS += recorder/src/service/post-process/modules/frame-skip
if !DISABLE_DISPLAY
BUILD_COMPONENTS += display/src/service
BUILD_COMPONENTS += display/test/samples
BUILD_COMPONENTS += display/test/gtest
endif
BUILD_COMPONENTS += recorder/src/client
BUILD_COMPONENTS += recorder/src/service
BUILD_COMPONENTS += recorder/test/samples
BUILD_COMPONENTS += recorder/test/gtest
if !DISABLE_TIMELAPSE_APP
BUILD_COMPONENTS += recorder/test/timelapse
endif
if !DISABLE_VIDEO_TIMELAPSE
BUILD_COMPONENTS += recorder/test/videotimelapse
endif
if !DISABLE_PLAYER_SERVICE
BUILD_COMPONENTS += player/src/client
BUILD_COMPONENTS += player/src/service
BUILD_COMPONENTS += player/test/demuxer
BUILD_COMPONENTS += player/test/samples
BUILD_COMPONENTS += player/test/gtest
endif
if !DISABLE_SYSTEM_CLIENT
BUILD_COMPONENTS += system/src/client
endif
if !DISABLE_SYSTEM_SERVICE
BUILD_COMPONENTS += system/src/service
endif
if !DISABLE_SYSTEM_TEST
BUILD_COMPONENTS += system/test/samples
endif
if !DISABLE_TRANSCODE_APP
BUILD_COMPONENTS += player/test/transcode_app
endif
if !DISABLE_SCREENNAIL_APP
BUILD_COMPONENTS += player/test/screen-nail
endif
BUILD_COMPONENTS += qmmf-server


SUBDIRS = $(BUILD_COMPONENTS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = qmmf-sdk.pc
EXTRA_DIST = $(pkgconfig_DATA)
