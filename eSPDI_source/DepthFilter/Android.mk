LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/../ \
    $(LOCAL_PATH)/../libopencl_stub/include

LOCAL_SHARED_LIBRARIES += libESPDI

LOCAL_WHOLE_STATIC_LIBRARIES += OpenCL

LOCAL_SRC_FILES := \
  $(LOCAL_PATH)/src/DepthmapFilter.cpp

LOCAL_MODULE := depthfilter

include $(BUILD_SHARED_LIBRARY)
