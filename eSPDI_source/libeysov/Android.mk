#owner:2230
#version:1.1.6.2
#min file_id:4
#current file_id:5



######################################################################
# libeysov.a
######################################################################
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/ \
    $(LOCAL_PATH)/../ \
     $(LOCAL_PATH)/../libMetaVideo \

LOCAL_EXPORT_C_INCLUDES := \
    $(LOCAL_PATH)/ \


LOCAL_CFLAGS := $(LOCAL_C_INCLUDES:%=-I%)
LOCAL_CFLAGS += -DANDROID_NDK
LOCAL_CFLAGS += -DLOG_NDEBUG
LOCAL_CFLAGS += -DEYS3D_NDEBUG

LOCAL_EXPORT_LDLIBS := -llog
LOCAL_SHARED_LIBRARIES += libMetaVideo
LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := \
    fisheye360_api_cc.cpp

LOCAL_MODULE := libeysov_static
include $(BUILD_STATIC_LIBRARY)

######################################################################
# libeysov.so
######################################################################
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_EXPORT_LDLIBS += -llog
LOCAL_LDLIBS += -latomic

LOCAL_WHOLE_STATIC_LIBRARIES = libeysov_static

LOCAL_MODULE := libeysov
include $(BUILD_SHARED_LIBRARY)
