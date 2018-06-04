LOCAL_PATH := $(call my-dir)

CORE_DIR     := $(LOCAL_PATH)/../..
LIBRETRO_DIR := $(CORE_DIR)/libretro

include $(LIBRETRO_DIR)/Makefile.common

COREFLAGS := -DHAVE_STDINT_H -DLSB_FIRST -D__LIBRETRO__ -DFINAL_VERSION -DC_CORE -DNO_LINK -DFRONTEND_SUPPORTS_RGB565 -DTILED_RENDERING $(INCFLAGS)

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
  COREFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

include $(CLEAR_VARS)
LOCAL_MODULE    := retro
LOCAL_SRC_FILES := $(SOURCES_CXX) 
LOCAL_CXXFLAGS  := $(COREFLAGS)
LOCAL_LDFLAGS   := -Wl,-version-script=$(LIBRETRO_DIR)/link.T
include $(BUILD_SHARED_LIBRARY)
