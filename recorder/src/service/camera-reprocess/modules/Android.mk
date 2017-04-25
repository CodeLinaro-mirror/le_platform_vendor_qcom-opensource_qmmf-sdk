LOCAL_PATH := $(call my-dir)

MY_PATH := $(LOCAL_PATH)

include $(MY_PATH)/jpeg-encoder/Android.mk

include $(MY_PATH)/haze-buster/Android.mk

include $(MY_PATH)/simple/Android.mk

include $(MY_PATH)/copy/Android.mk

include $(MY_PATH)/camera-hal/Android.mk
