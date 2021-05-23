LOCAL_PATH   := $(call my-dir)
CORE_DIR     := $(LOCAL_PATH)/../..
LIBRETRO_DIR := $(CORE_DIR)/libretro

FRONTEND_SUPPORTS_RGB565 := 1
TILED_RENDERING          := 1
NO_LINK                  := 1

include $(LIBRETRO_DIR)/Makefile.common

COREFLAGS := -DHAVE_STDINT_H $(VBA_DEFINES) $(INCFLAGS)

VBAM_VERSION := $(shell sed -En 's/.*\[([0-9]+[^]]+).*/\1/p; T; q' ../../CHANGELOG.md 2>/dev/null)
COREFLAGS += -DVBAM_VERSION=\"$(VBAM_VERSION)\"
TAG_COMMIT     := $(shell git rev-list -n 1 v$(VBAM_VERSION) --abbrev-commit 2>/dev/null)
CURRENT_COMMIT := $(shell git rev-parse --short HEAD 2>/dev/null)

ifneq ($(CURRENT_COMMIT),$(TAG_COMMIT))
COREFLAGS += -DGIT_COMMIT=\"$(CURRENT_COMMIT)\"
endif

ifeq ($(NO_LINK), 1)
COREFLAGS += -DNO_LINK
endif

include $(CLEAR_VARS)
LOCAL_MODULE    := retro
LOCAL_SRC_FILES := $(SOURCES_CXX) 
LOCAL_CXXFLAGS  := $(COREFLAGS)
LOCAL_LDFLAGS   := -Wl,-version-script=$(LIBRETRO_DIR)/link.T
include $(BUILD_SHARED_LIBRARY)
