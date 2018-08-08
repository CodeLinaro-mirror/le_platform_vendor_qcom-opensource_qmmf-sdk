
ifeq ($(TARGET_BOARD_PLATFORM),msm8x53)
DISABLE_DISPLAY := 1
endif

include $(call all-subdir-makefiles)
