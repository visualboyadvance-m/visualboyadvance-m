LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CFLAGS +=  -DANDROID_X86
endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DANDROID_MIPS
endif

LOCAL_MODULE    := libretro
LOCAL_SRC_FILES    = ../../src/gba.cpp ../../src/memory.cpp ../../src/sound.cpp ../../libretro/libretro.cpp
LOCAL_CFLAGS = -DINLINE=inline -DHAVE_STDINT_H -DHAVE_INTTYPES_H -DSPEEDHAX -DLSB_FIRST -D__LIBRETRO__ -DFRONTEND_SUPPORTS_RGB565
LOCAL_C_INCLUDES = ../src

include $(BUILD_SHARED_LIBRARY)
