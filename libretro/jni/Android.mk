LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libavformat
CORE_DIR    := ..
ROOTDIR     := $(CORE_DIR)/../
FFMPEGDIR   := $(ROOTDIR)/ffmpeg
FFMPEGLIBDIR   := $(FFMPEGDIR)/android/armv7/lib
LOCAL_SRC_FILES := $(FFMPEGLIBDIR)/libavformat.a

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE    := libavcodec
CORE_DIR    := ..
ROOTDIR     := $(CORE_DIR)/../
FFMPEGDIR   := $(ROOTDIR)/ffmpeg
FFMPEGLIBDIR   := $(FFMPEGDIR)/android/armv7/lib
LOCAL_SRC_FILES := $(FFMPEGLIBDIR)/libavcodec.a

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE    := libavutil
CORE_DIR    := ..
ROOTDIR     := $(CORE_DIR)/../
FFMPEGDIR   := $(ROOTDIR)/ffmpeg
FFMPEGLIBDIR   := $(FFMPEGDIR)/android/armv7/lib
LOCAL_SRC_FILES := $(FFMPEGLIBDIR)/libavutil.a

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE    := libswresample
CORE_DIR    := ..
ROOTDIR     := $(CORE_DIR)/../
FFMPEGDIR   := $(ROOTDIR)/ffmpeg
FFMPEGLIBDIR   := $(FFMPEGDIR)/android/armv7/lib
LOCAL_SRC_FILES := $(FFMPEGLIBDIR)/libswresample.a

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE    := libswscale
CORE_DIR    := ..
ROOTDIR     := $(CORE_DIR)/../
FFMPEGDIR   := $(ROOTDIR)/ffmpeg
FFMPEGLIBDIR   := $(FFMPEGDIR)/android/armv7/lib
LOCAL_SRC_FILES := $(FFMPEGLIBDIR)/libswscale.a

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)


ifeq ($(TARGET_ARCH),arm)
  LOCAL_CFLAGS += -DANDROID_ARM -DARM -DARMEABI_V7A
  LOCAL_ARM_MODE := arm
  LOCAL_ARM_NEON := true
  LOCAL_CLFAGS += -marm
endif

ifeq ($(TARGET_ARCH),x86)
  LOCAL_CFLAGS += -DANDROID_X86 -D_M_IX86 -fomit-frame-pointer -mtune=atom -mfpmath=sse -mssse3
endif

ifeq ($(TARGET_ARCH),mips)
  LOCAL_CFLAGS += -DANDROID_MIPS
endif

CORE_DIR    := ..
ROOTDIR     := $(CORE_DIR)/../
FFMPEGDIR   := $(ROOTDIR)/ffmpeg
LIBRETRODIR := $(ROOTDIR)/libretro
COREDIR     := $(ROOTDIR)/Core
COMMONDIR   := $(ROOTDIR)/Common
GPUCOMMONDIR:= $(ROOTDIR)/GPU/Common
GPUDIR      := $(ROOTDIR)/GPU
NATIVEDIR   := $(ROOTDIR)/native
EXTDIR      := $(ROOTDIR)/ext

FFMPEGINCFLAGS := -I$(FFMPEGDIR)/android/armv7/include
FFMPEGLIBDIR   := $(FFMPEGDIR)/android/armv7/lib
FFMPEGLIBS  += libavformat libavcodec libavutil libswresample libswscale

WITH_DYNAREC = arm
GLES = 1

INCFLAGS += $(FFMPEGINCFLAGS)

LOCAL_MODULE := retro

platform := android
HAVE_NEON := 1

include $(CORE_DIR)/Makefile.common

COREFLAGS := -DINLINE="inline" -DPPSSPP -DUSE_FFMPEG -DMOBILE_DEVICE -DBAKE_IN_GIT -DDYNAREC -D__LIBRETRO__ -D__arm__ -DARM_ASM -D__NEON_OPT__ -DUSING_GLES2 -D__STDC_CONSTANT_MACROS
LOCAL_SRC_FILES = $(SOURCES_CXX) $(SOURCES_C) $(ASMFILES)
LOCAL_CPPFLAGS := -Wall -std=gnu++11 -Wno-literal-suffix $(INCFLAGS) $(COREFLAGS)
LOCAL_CFLAGS := -O2 -ffast-math -DANDROID $(INCFLAGS) $(COREFLAGS)
LOCAL_C_INCLUDES += $(INCFLAGS)
LOCAL_LDLIBS += -lz -llog -lGLESv2 -lEGL
LOCAL_STATIC_LIBRARIES += $(FFMPEGLIBS)

include $(BUILD_SHARED_LIBRARY)

