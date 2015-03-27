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

VBADIR = ../../

LOCAL_MODULE    := libretro
LOCAL_SRC_FILES    = $(VBADIR)/gba/agbprint.cpp \
                     $(VBADIR)/gba/armdis.cpp \
                     $(VBADIR)/gba/bios.cpp \
                     $(VBADIR)/gba/Cheats.cpp \
                     $(VBADIR)/gba/CheatSearch.cpp \
                     $(VBADIR)/gba/EEprom.cpp \
                     $(VBADIR)/gba/elf.cpp \
                     $(VBADIR)/gba/ereader.cpp \
                     $(VBADIR)/gba/Flash.cpp \
                     $(VBADIR)/gba/GBA-arm.cpp \
                     $(VBADIR)/gba/GBA.cpp \
                     $(VBADIR)/gba/gbafilter.cpp \
                     $(VBADIR)/gba/GBAGfx.cpp \
                     $(VBADIR)/gba/GBALink.cpp \
                     $(VBADIR)/gba/GBASockClient.cpp \
                     $(VBADIR)/gba/GBA-thumb.cpp \
                     $(VBADIR)/gba/Globals.cpp \
                     $(VBADIR)/gba/Mode0.cpp \
                     $(VBADIR)/gba/Mode1.cpp \
                     $(VBADIR)/gba/Mode2.cpp \
                     $(VBADIR)/gba/Mode3.cpp \
                     $(VBADIR)/gba/Mode4.cpp \
                     $(VBADIR)/gba/Mode5.cpp \
                     $(VBADIR)/gba/remote.cpp \
                     $(VBADIR)/gba/RTC.cpp \
                     $(VBADIR)/gba/Sound.cpp \
                     $(VBADIR)/gba/Sram.cpp \
                     $(VBADIR)/apu/Blip_Buffer.cpp \
                     $(VBADIR)/apu/Effects_Buffer.cpp \
                     $(VBADIR)/apu/Gb_Apu.cpp \
                     $(VBADIR)/apu/Gb_Apu_State.cpp \
                     $(VBADIR)/apu/Gb_Oscs.cpp \
                     $(VBADIR)/apu/Multi_Buffer.cpp \
                     $(VBADIR)/libretro/libretro.cpp \
                     $(VBADIR)/libretro/UtilRetro.cpp \
                     $(VBADIR)/libretro/SoundRetro.cpp \
                     $(VBADIR)/libretro/scrc32.cpp

LOCAL_CFLAGS = -O3 -DINLINE=inline -DHAVE_STDINT_H -DHAVE_INTTYPES_H -DLSB_FIRST -D__LIBRETRO__ -DFINAL_VERSION -DC_CORE -DUSE_GBA_ONLY -DNO_LINK -DFRONTEND_SUPPORTS_RGB565 -DTILED_RENDERING
LOCAL_C_INCLUDES = $(VBADIR)

include $(BUILD_SHARED_LIBRARY)
