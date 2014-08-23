DEBUG=0
PERF_TEST=0
HAVE_SHARED_CONTEXT=0
SINGLE_THREAD=0

DYNAFLAGS :=
INCFLAGS  :=
COREFLAGS :=
CPUFLAGS  :=

UNAME=$(shell uname -a)

# Cross compile ?

ifeq (,$(ARCH))
  ARCH = $(shell uname -m)
endif

# Target Dynarec
WITH_DYNAREC = $(ARCH)
FFMPEG_ARCH = $(ARCH)

ifeq ($(ARCH), i386)
   WITH_DYNAREC = x86
   FFMPEG_ARCH = x86
endif

ifeq ($(platform),)
   platform = unix
   ifeq ($(UNAME),)
      platform = win
   else ifneq ($(findstring MINGW,$(UNAME)),)
      platform = win
   else ifneq ($(findstring Darwin,$(UNAME)),)
      platform = osx
   else ifneq ($(findstring win,$(UNAME)),)
      platform = win
   endif
endif


# Dirs
ROOTDIR        = ..
FFMPEGDIR      = $(ROOTDIR)/ffmpeg
LIBRETRODIR    = $(ROOTDIR)/libretro
COREDIR        = $(ROOTDIR)/Core
COMMONDIR      = $(ROOTDIR)/Common
GPUCOMMONDIR   = $(ROOTDIR)/GPU/Common
GPUDIR         = $(ROOTDIR)/GPU
NATIVEDIR      = $(ROOTDIR)/native
EXTDIR         = $(ROOTDIR)/ext

TARGET_NAME := ppsspp
CC_AS ?= $(CC)
FFMPEGINCFLAGS :=
FFMPEGLIBDIR :=
FFMPEGLIBS :=

ifneq (,$(findstring unix,$(platform)))
   TARGET := $(TARGET_NAME)_libretro.so
   LDFLAGS += -shared -Wl,--version-script=link.T -Wl,--no-undefined
   
   FFMPEGINCFLAGS += -I$(FFMPEGDIR)/linux/$(FFMPEG_ARCH)/include
   FFMPEGLIBDIR :=   $(FFMPEGDIR)/linux/$(FFMPEG_ARCH)/lib
   FFMPEGLDFLAGS += -L$(FFMPEGLIBDIR) -lavformat -lavcodec -lavutil -lswresample -lswscale
   fpic = -fPIC
ifneq (,$(findstring gles,$(platform)))
   GLES = 1
   GL_LIB := -lGLESv2
else
   GL_LIB := -lGL
endif
   PLATFORM_EXT := unix
else ifneq (,$(findstring rpi,$(platform)))
   TARGET := $(TARGET_NAME)_libretro.so
   LDFLAGS += -shared -Wl,--version-script=link.T
   fpic = -fPIC
   GLES = 1
   GL_LIB := -lGLESv2
   INCFLAGS += -I/opt/vc/include
   CPUFLAGS += -DARMv5_ONLY 
   PLATFORM_EXT := unix
   WITH_DYNAREC=arm
else ifneq (,$(findstring imx6,$(platform)))
   TARGET := $(TARGET_NAME)_libretro.so
   LDFLAGS += -shared -Wl,--version-script=link.T
   fpic = -fPIC
   GLES = 1
   GL_LIB := -lGLESv2
   CPUFLAGS += 
   PLATFORM_EXT := unix
   WITH_DYNAREC=arm
   HAVE_NEON=1
	FFMPEGINCFLAGS += -I$(FFMPEGDIR)/linux/armv7/include
	FFMPEGLIBDIR :=   $(FFMPEGDIR)/linux/armv7/lib
	FFMPEGLDFLAGS += -L$(FFMPEGLIBDIR) -lavformat -lavcodec -lavutil -lswresample -lswscale
else ifneq (,$(findstring osx,$(platform)))
   TARGET := $(TARGET_NAME)_libretro.dylib
   LDFLAGS += -dynamiclib
   OSXVER = `sw_vers -productVersion | cut -c 4`
ifneq ($(shell sw_vers | grep -c 10.9),1)
   LDFLAGS += -mmacosx-version-min=10.5
endif
   fpic = -fPIC

   FFMPEGINCFLAGS += -I$(FFMPEGDIR)/macosx/$(FFMPEG_ARCH)/include
   FFMPEGLIBDIR := $(FFMPEGDIR)/macosx/$(FFMPEG_ARCH)/lib
   FFMPEGLDFLAGS += -liconv -L$(FFMPEGLIBDIR) -lavformat -lavcodec -lavutil -lswresample -lswscale
   PLATCFLAGS += -D__MACOSX__
   GL_LIB := -framework OpenGL
   PLATFORM_EXT := unix
else ifneq (,$(findstring ios,$(platform)))
   TARGET := $(TARGET_NAME)_libretro_ios.dylib
   PLATCFLAGS += -DIOS -marm -mllvm -arm-reserve-r9
   LDFLAGS += -dynamiclib -marm
   fpic = -fPIC
   GLES = 1
   GL_LIB := -framework OpenGLES

   CC = clang -arch armv7 -isysroot $(IOSSDK)
   CC_AS = perl ./tools/gas-preprocessor.pl $(CC)
   CXX = clang++ -arch armv7 -isysroot $(IOSSDK)
   OSXVER = `sw_vers -productVersion | cut -c 4`
ifneq ($(OSXVER),9)
   CC += -miphoneos-version-min=5.0
   CC_AS += -miphoneos-version-min=5.0
   CXX += -miphoneos-version-min=5.0
   PLATCFLAGS += -miphoneos-version-min=5.0
endif
   PLATCFLAGS += -DIOS -DHAVE_POSIX_MEMALIGN
   CPUFLAGS += -DARMv5_ONLY -DARM
   PLATFORM_EXT := unix
   WITH_DYNAREC=arm
else ifneq (,$(findstring android,$(platform)))
   fpic = -fPIC
   TARGET := $(TARGET_NAME)_libretro_android.so
   LDFLAGS += -shared -Wl,--version-script=link.T -Wl,--no-undefined -Wl,--warn-common
   GL_LIB := -lGLESv2

   CC = arm-linux-androideabi-gcc
   CXX = arm-linux-androideabi-g++
   WITH_DYNAREC=arm
   GLES = 1
   PLATCFLAGS += -DANDROID
   CPUCFLAGS  += 
   HAVE_NEON = 1
   CPUFLAGS += -marm -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp -D__arm__ -DARM_ASM -D__NEON_OPT
   CFLAGS += -DANDROID
   
   PLATFORM_EXT := unix
else ifeq ($(platform), qnx)
   fpic = -fPIC
   TARGET := $(TARGET_NAME)_libretro_qnx.so
   LDFLAGS += -shared -Wl,--version-script=link.T -Wl,--no-undefined -Wl,--warn-common
   GL_LIB := -lGLESv2

   CC = qcc -Vgcc_ntoarmv7le
   CC_AS = qcc -Vgcc_ntoarmv7le
   CXX = QCC -Vgcc_ntoarmv7le
   AR = QCC -Vgcc_ntoarmv7le
	WITH_DYNAREC=arm
   GLES = 1
   PLATCFLAGS += -D__BLACKBERRY_QNX__
   HAVE_NEON = 1
   CPUFLAGS += -marm -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=softfp -D__arm__ -DARM_ASM -D__NEON_OPT
   CFLAGS += -D__QNX__
   
   PLATFORM_EXT := unix
else ifneq (,$(findstring armv,$(platform)))
   CC = gcc
   CXX = g++
   TARGET := $(TARGET_NAME)_libretro.so
   fpic := -fPIC
   SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined
   INCFLAGS += -I.
   WITH_DYNAREC=arm
ifneq (,$(findstring gles,$(platform)))
   GLES := 1
else
   GL_LIB := -lGL
endif
ifneq (,$(findstring cortexa8,$(platform)))
   CPUFLAGS += -marm -mcpu=cortex-a8
else ifneq (,$(findstring cortexa9,$(platform)))
   CPUFLAGS += -marm -mcpu=cortex-a9
endif
   CPUFLAGS += -marm
ifneq (,$(findstring neon,$(platform)))
   CPUFLAGS += -mfpu=neon
   HAVE_NEON = 1
endif
ifneq (,$(findstring softfloat,$(platform)))
   CPUFLAGS += -mfloat-abi=softfp
else ifneq (,$(findstring hardfloat,$(platform)))
   CPUFLAGS += -mfloat-abi=hard
endif
   PLATCFLAGS += -DARM
else ifeq ($(platform), emscripten)
   TARGET := $(TARGET_NAME)_libretro_emscripten.bc
   GLES := 1
   CPUFLAGS += 
   PLATCFLAGS += -DCC_resampler=mupen_CC_resampler -Dsinc_resampler=mupen_sinc_resampler \
               -Drglgen_symbol_map=mupen_rglgen_symbol_map -Dmain_exit=mupen_main_exit \
               -Dadler32=mupen_adler32 -Drarch_resampler_realloc=mupen_rarch_resampler_realloc \
               -Daudio_convert_s16_to_float_C=mupen_audio_convert_s16_to_float_C -Daudio_convert_float_to_s16_C=mupen_audio_convert_float_to_s16_C \
               -Daudio_convert_init_simd=mupen_audio_convert_init_simd -Drglgen_resolve_symbols_custom=mupen_rglgen_resolve_symbols_custom \
               -Drglgen_resolve_symbols=mupen_rglgen_resolve_symbols
   PLATFORM_EXT := unix
   #HAVE_SHARED_CONTEXT := 1
else ifneq (,$(findstring win,$(platform)))
   TARGET := $(TARGET_NAME)_libretro.dll
   LDFLAGS += -shared -static-libgcc -static-libstdc++ -Wl,--version-script=link.T -lwinmm -lgdi32
   GL_LIB := -lopengl32
   PLATFORM_EXT := win32
   CC = gcc
   CXX = g++
endif

include Makefile.common

ifeq ($(PERF_TEST), 1)
   COREFLAGS += -DPERF_TEST
endif

ifeq ($(HAVE_SHARED_CONTEXT), 1)
   COREFLAGS += -DHAVE_SHARED_CONTEXT
endif

ifeq ($(SINGLE_THREAD), 1)
   COREFLAGS += -DSINGLE_THREAD
endif

ifeq ($(GLES), 1)
   GLFLAGS += -DGLES -DHAVE_OPENGLES2 -DUSING_GLES2
else
   GLFLAGS += -DHAVE_OPENGL
endif

COREFLAGS += -D__LIBRETRO__ -DINLINE="inline" -DPPSSPP -DUSE_FFMPEG -DBAKE_IN_GIT

ifeq ($(DEBUG), 1)
   CPUOPTS += -O0 -g
   CPUOPTS += -DOPENGL_DEBUG
else
   CPUOPTS += -O3 -DNDEBUG
endif

### Finalize ###
OBJECTS    += $(CXXFILES:.cpp=.o) $(CFILES:.c=.o)
CXXFLAGS   +=  -std=c++11 $(CPUOPTS) $(COREFLAGS) $(INCFLAGS) $(PLATCFLAGS) $(fpic) $(PLATCFLAGS) $(CPUFLAGS) $(GLFLAGS) $(DYNAFLAGS) 
CFLAGS     +=  $(CPUOPTS) $(COREFLAGS) $(INCFLAGS) $(PLATCFLAGS) $(fpic) $(PLATCFLAGS) $(CPUFLAGS) $(GLFLAGS) $(DYNAFLAGS)

LDFLAGS    += -lm -lpthread -ldl -lz $(FFMPEGLDFLAGS) $(fpic)

all: $(TARGET)

%.o: %.S
	$(CC_AS) $(CFLAGS) -c $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) $(LDFLAGS) $(GL_LIB)

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean
