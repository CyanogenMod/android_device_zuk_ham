# Copyright (C) 2015 The CyanogenMod Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := fingerprint.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -std=c99
LOCAL_SRC_FILES := \
     fpd_sm.c \
     fpd_client.c \
     fingerprint.c

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CFLAGS := -std=c99
LOCAL_MODULE := fpd_client_test_tool
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_MODULE_TAGS := optional eng

LOCAL_SRC_FILES := \
     fpd_sm.c \
     fpd_client.c \
     test/fpd_client_test_tool.c
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_CFLAGS := -std=c99
LOCAL_MODULE := fpd_sm_test_tool
LOCAL_SHARED_LIBRARIES := liblog libcutils libncurses
LOCAL_STATIC_LIBRARIES := libreadline libhistory 
LOCAL_MODULE_TAGS := optional eng
LOCAL_C_INCLUDES += external/bash/lib
LOCAL_SRC_FILES := \
     fpd_sm.c \
     fpd_client.c \
     test/fpd_sm_test_tool.c
include $(BUILD_EXECUTABLE)
