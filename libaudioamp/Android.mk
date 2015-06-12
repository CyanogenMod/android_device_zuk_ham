LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += \
     $(call project-path-for,qcom-audio)/hal/msm8974/ \
     $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
LOCAL_SHARED_LIBRARIES := liblog libutils libcutils libtfa98xx
LOCAL_SRC_FILES := audio_amplifier.c
LOCAL_MODULE := libaudioamp
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
