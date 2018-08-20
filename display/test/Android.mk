ifneq ($(DISABLE_DISPLAY),1)
ifneq ($(TARGET_BOARD_PLATFORM),qcs605)
include $(call all-subdir-makefiles)
endif
endif
