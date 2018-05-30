LOCAL_CPP_EXTENSION := .cc

LOCAL_CFLAGS := -Wall -Wextra -Werror -std=c++14 -fexceptions
# TODO functions have unused input parameters
LOCAL_CFLAGS += -Wno-unused-parameter
# Suppress unused variable caused by assert only for release variant
ifeq (userdebug,$(TARGET_BUILD_VARIANT))
LOCAL_CFLAGS += -UNDEBUG
else
LOCAL_CFLAGS += -Wno-unused-variable
endif

# ANDROID version check
ANDROID_MAJOR_VERSION :=$(shell echo $(PLATFORM_VERSION) | cut -f1 -d.)
IS_ANDROID_O_OR_ABOVE :=$(shell test $(ANDROID_MAJOR_VERSION) -gt 8 -o $(ANDROID_MAJOR_VERSION) -eq 8 && echo true)
ifeq ($(IS_ANDROID_O_OR_ABOVE),true)
LOCAL_CFLAGS += -DANDROID_O_OR_ABOVE
endif #ANDROID version check

LOCAL_C_INCLUDES := $(QMMF_SDK_TOP_SRCDIR)/include
LOCAL_C_INCLUDES += $(QMMF_SDK_TOP_SRCDIR)
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include

# Header files required for O MR1
ifeq ($(IS_ANDROID_O_OR_ABOVE),true)
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/libs/nativewindow/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/libs/nativebase/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/libs/arect/include
endif

LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_SHARED_LIBRARIES := libcutils libutils libdl liblog

LOCAL_EXPORT_C_INCLUDE_DIRS := $(QMMF_SDK_TOP_SRCDIR)/include

LOCAL_32_BIT_ONLY := true

# Enable libs/bins installation into vendor
ifeq ($(IS_ANDROID_O_OR_ABOVE),true)
LOCAL_VENDOR_MODULE := true
endif #LOCAL_VENDOR_MODULE

# Disable jpeg postproc
ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
DISABLE_PP_JPEG := 1
LOCAL_CFLAGS += -DDISABLE_PP_JPEG
endif #DISABLE_PP_JPEG

# Disable Video LPM
ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
LOCAL_CFLAGS += -DDISABLE_VID_LPM
endif #DISABLE_VID_LPM

# Disable Video QP Range
ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
LOCAL_CFLAGS += -DDISABLE_VID_QP_RANGE
endif #DISABLE_VID_QP_RANGE

# Disable Op Modes
ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
LOCAL_CFLAGS += -DDISABLE_OP_MODES
endif #DISABLE_OP_MODES

# Disable display service
ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
DISABLE_DISPLAY := 1
LOCAL_CFLAGS += -DDISABLE_DISPLAY
USE_SURFACEFLINGER := 1
LOCAL_CFLAGS += -DUSE_SURFACEFLINGER
endif #DISABLE_DISPLAY

# Enable Gralloc1 support
ifeq ($(TARGET_USES_GRALLOC1),true)
LOCAL_CFLAGS += -DTARGET_USES_GRALLOC1
endif #TARGET_USES_GRALLOC1

# Enable Vendor Tag Descriptor
ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
LOCAL_CFLAGS += -DUSE_VENDOR_TAG_DESC
endif #VENDOR_TAG_DESC

# Disable MultiCamera Manager
ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
ENABLE_360 := 0
endif #ENABLE_360

# Set HFR Threshold values based on platform
ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
LOCAL_CFLAGS += -DHFR_THRESHOLD=90.0f
else
LOCAL_CFLAGS += -DHFR_THRESHOLD=30.0f
endif #HFR_THRESHOLD

ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
LOCAL_CFLAGS += -DUSE_FPS_IDX
endif #USE_FPS_IDX

# Jpeg Blob offset
ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
LOCAL_CFLAGS += -DJPEG_BLOB_OFFSET=0
else
LOCAL_CFLAGS += -DJPEG_BLOB_OFFSET=1
endif #JPEG_BLOB_OFFSET

# Enable MPEGH encoder support
ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
LOCAL_CFLAGS += -DMPEGH_ENCODER_SUPPORT
endif #MPEGH_ENCODER_SUPPORT

# Add extra input buffers for high fps
ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
LOCAL_CFLAGS += -DEXTRA_BUFFER_SUPPORT
endif #EXTRA_BUFFER_SUPPORT

# AEC timeout value in ms (if not set here, default value is set in src)
ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
LOCAL_CFLAGS += -DAEC_WAIT_TIMEOUT=750000000
else
LOCAL_CFLAGS += -DAEC_WAIT_TIMEOUT=500000000
endif #AEC_WAIT_TIMEOUT

# FLUSH_RESTART_NOTAVAILABLE
ifeq ($(TARGET_BOARD_PLATFORM),qcs605)
LOCAL_CFLAGS += -DFLUSH_RESTART_NOTAVAILABLE
endif #FLUSH_RESTART_NOTAVAILABLE
