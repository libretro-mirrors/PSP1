DEBUG=0
PERF_TEST=0
HAVE_SHARED_CONTEXT=0
SINGLE_THREAD=0

DYNAFLAGS :=
INCFLAGS  :=
COREFLAGS :=
CPUFLAGS  :=

UNAME=$(shell uname -a)

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

ifeq ($(firstword $(filter x86_64,$(UNAME))),x86_64)
	WITH_DYNAREC=x86_64
endif
ifeq ($(firstword $(filter amd64,$(UNAME))),amd64)
	WITH_DYNAREC=x86_64
endif

# Dirs
ROOTDIR        = ../
FFMPEGDIR      = ../ffmpeg
LIBRETRODIR    = ../libretro
COREDIR        = ../Core
COMMONDIR      = ../Common
GPUCOMMONDIR   = ../GPU/Common
GPUDIR         = ../GPU
NATIVEDIR      = ../native
EXTDIR         = ../ext

TARGET_NAME := ppsspp
CC_AS ?= $(CC)
FFMPEGINCFLAGS :=
FFMPEGLIBDIR :=
FFMPEGLIBS :=

ifneq (,$(findstring unix,$(platform)))
   TARGET := $(TARGET_NAME)_libretro.so
   LDFLAGS += -shared -Wl,--version-script=link.T -Wl,--no-undefined
   
	FFMPEGINCFLAGS += -I$(FFMPEGDIR)/linux/x86_64/include
	FFMPEGLIBDIR :=   $(FFMPEGDIR)/linux/x86_64/lib
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
else ifneq (,$(findstring osx,$(platform)))
   TARGET := $(TARGET_NAME)_libretro.dylib
   LDFLAGS += -dynamiclib
   OSXVER = `sw_vers -productVersion | cut -c 4`
ifneq ($(shell sw_vers | grep -c 10.9),1)
   LDFLAGS += -mmacosx-version-min=10.5
endif
   fpic = -fPIC

	FFMPEGINCFLAGS += -I$(FFMPEGDIR)/macosx/x86_64/include
	FFMPEGLIBDIR := $(FFMPEGDIR)/macosx/x86_64/lib
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



INCFLAGS := \
	-I$(ROOTDIR) \
	-I$(COMMONDIR) \
	-I$(LIBRETRODIR)/ext/glew \
	-I$(EXTDIR)/snappy \
	-I$(NATIVEDIR)/ext/rg_etc1 \
	-I$(NATIVEDIR)/ext/stb_vorbis \
	$(FFMPEGINCFLAGS) \
	-I$(EXTDIR)/cityhash \
	-I$(NATIVEDIR)/ext/libzip \
	-I$(NATIVEDIR)/ext \
	-I$(NATIVEDIR) \
	-I$(EXTDIR)/libkirk \
	-I$(EXTDIR)/xbrz \
	-I$(EXTDIR)/xxhash

CXXFILES += $(COMMONDIR)/ABI.cpp
CFILES   += $(LIBRETRODIR)/ext/glew/glew.c
CXXFILES += $(NATIVEDIR)/ext/rg_etc1/rg_etc1.cpp
CXXFILES += $(EXTDIR)/snappy/snappy-c.cpp
CFILES   += $(NATIVEDIR)/ext/stb_vorbis/stb_vorbis.c \
				$(NATIVEDIR)/ext/libpng17/png.c
CXXFILES += $(NATIVEDIR)/ext/vjson/json.cpp \
				$(EXTDIR)/snappy/snappy.cpp
CFILES   += $(NATIVEDIR)/ext/libpng17/pngerror.c
CFILES    += $(NATIVEDIR)/ext/libpng17/pngget.c \
				 $(NATIVEDIR)/ext/libzip/zip_name_locate.c
CXXFILES += $(COMMONDIR)/Thunk.cpp
CFILES   += $(NATIVEDIR)/ext/libpng17/pngmem.c \
				$(NATIVEDIR)/ext/libpng17/pngpread.c 
CXXFILES += $(NATIVEDIR)/ext/cityhash/city.cpp \
				$(COMMONDIR)/ChunkFile.cpp
CFILES   += $(NATIVEDIR)/ext/libpng17/pngread.c
CXXFILES += $(COMMONDIR)/ConsoleListener.cpp \
				$(COMMONDIR)/Crypto/md5.cpp \
				$(COMMONDIR)/Crypto/sha1.cpp
CFILES   += $(NATIVEDIR)/ext/libzip/mkstemp.c
CXXFILES += $(COMMONDIR)/FileUtil.cpp
CFILES   += $(NATIVEDIR)/ext/libzip/zip_add.c \
				$(EXTDIR)/libkirk/AES.c \
				$(NATIVEDIR)/ext/libzip/zip_add_dir.c
CXXFILES += $(COMMONDIR)/KeyMap.cpp
CFILES   += $(NATIVEDIR)/ext/libzip/zip_close.c
CXXFILES += $(COMMONDIR)/LogManager.cpp
CFILES   += $(EXTDIR)/libkirk/amctrl.c \
				$(NATIVEDIR)/ext/libpng17/pngrio.c \
				$(NATIVEDIR)/ext/libpng17/pngrtran.c \
				$(NATIVEDIR)/ext/libzip/zip_delete.c \
				$(NATIVEDIR)/ext/libzip/zip_dirent.c \
				$(EXTDIR)/libkirk/SHA1.c \
				$(NATIVEDIR)/ext/libzip/zip_entry_free.c \
				$(NATIVEDIR)/ext/libzip/zip_entry_new.c 
CXXFILES += $(COMMONDIR)/MemArena.cpp
CFILES   += $(NATIVEDIR)/ext/libzip/zip_err_str.c \
				$(NATIVEDIR)/ext/libzip/zip_error.c \
				$(NATIVEDIR)/ext/libzip/zip_error_clear.c \
				$(NATIVEDIR)/ext/libzip/zip_error_get.c
CXXFILES += $(COMMONDIR)/MemoryUtil.cpp
CFILES   += $(EXTDIR)/libkirk/bn.c \
				$(NATIVEDIR)/ext/libzip/zip_error_get_sys_type.c \
				$(NATIVEDIR)/ext/libzip/zip_error_strerror.c \
				$(EXTDIR)/libkirk/ec.c \
				$(NATIVEDIR)/ext/libzip/zip_error_to_str.c \
				$(NATIVEDIR)/ext/libzip/zip_fclose.c 
CXXFILES += $(COMMONDIR)/Misc.cpp
CFILES   += $(NATIVEDIR)/ext/libzip/zip_file_error_clear.c
CXXFILES += $(COMMONDIR)/MsgHandler.cpp
CFILES   += $(EXTDIR)/libkirk/kirk_engine.c \
				$(NATIVEDIR)/ext/libzip/zip_file_error_get.c \
				$(NATIVEDIR)/ext/libzip/zip_file_get_offset.c \
				$(NATIVEDIR)/ext/libzip/zip_file_strerror.c
CXXFILES += $(COMMONDIR)/StringUtils.cpp
CFILES   += $(NATIVEDIR)/ext/libzip/zip_filerange_crc.c \
				$(NATIVEDIR)/ext/libzip/zip_fopen.c
CXXFILES += $(EXTDIR)/xbrz/xbrz.cpp \
				$(COMMONDIR)/ThreadPools.cpp
CFILES   += $(NATIVEDIR)/ext/libzip/zip_fopen_index.c \
				$(NATIVEDIR)/ext/libzip/zip_fread.c \
				$(NATIVEDIR)/ext/libzip/zip_free.c
CXXFILES += $(COMMONDIR)/Timer.cpp
CFILES   += $(NATIVEDIR)/ext/libzip/zip_get_archive_comment.c \
				$(NATIVEDIR)/ext/libzip/zip_get_archive_flag.c \
				$(NATIVEDIR)/ext/libpng17/pngrutil.c \
				$(NATIVEDIR)/ext/libzip/zip_get_file_comment.c \
				$(NATIVEDIR)/ext/libpng17/pngset.c \
				$(NATIVEDIR)/ext/libzip/zip_get_name.c \
				$(NATIVEDIR)/ext/libzip/zip_get_num_files.c \
				$(NATIVEDIR)/ext/libzip/zip_memdup.c \
				$(NATIVEDIR)/ext/libzip/zip_new.c \
				$(NATIVEDIR)/ext/libzip/zip_open.c \
				$(NATIVEDIR)/ext/libzip/zip_rename.c \
				$(NATIVEDIR)/ext/libpng17/pngtest.c \
				$(NATIVEDIR)/ext/libzip/zip_replace.c \
				$(EXTDIR)/xxhash.c \
				$(NATIVEDIR)/ext/libzip/zip_set_archive_comment.c \
				$(NATIVEDIR)/ext/libzip/zip_set_archive_flag.c \
				$(NATIVEDIR)/ext/libzip/zip_set_file_comment.c \
				$(NATIVEDIR)/ext/libzip/zip_set_name.c \
				$(NATIVEDIR)/ext/libzip/zip_source_buffer.c \
				$(NATIVEDIR)/ext/libzip/zip_source_file.c \
				$(NATIVEDIR)/ext/libzip/zip_source_filep.c \
				$(NATIVEDIR)/ext/libpng17/pngtrans.c \
				$(NATIVEDIR)/ext/libzip/zip_source_free.c
CXXFILES += $(GPUCOMMONDIR)/VertexDecoderCommon.cpp
CFILES   += $(NATIVEDIR)/ext/libzip/zip_source_function.c \
				$(NATIVEDIR)/ext/libzip/zip_source_zip.c \
				$(NATIVEDIR)/ext/libzip/zip_stat.c \
				$(NATIVEDIR)/ext/libzip/zip_stat_index.c \
				$(NATIVEDIR)/ext/libpng17/pngwio.c \
				$(NATIVEDIR)/ext/libzip/zip_stat_init.c \
				$(NATIVEDIR)/ext/libpng17/pngwrite.c \
				$(NATIVEDIR)/ext/libzip/zip_strerror.c \
				$(NATIVEDIR)/ext/libzip/zip_unchange.c \
				$(NATIVEDIR)/ext/libzip/zip_unchange_all.c
CXXFILES += $(GPUCOMMONDIR)/TransformCommon.cpp
CFILES   += $(NATIVEDIR)/ext/libpng17/pngwtran.c \
				$(NATIVEDIR)/ext/libzip/zip_unchange_archive.c \
				$(NATIVEDIR)/ext/libzip/zip_unchange_data.c
CXXFILES += $(GPUCOMMONDIR)/IndexGenerator.cpp
CFILES   += $(NATIVEDIR)/ext/libpng17/pngwutil.c
CXXFILES += $(GPUCOMMONDIR)/TextureDecoder.cpp \
				$(GPUCOMMONDIR)/PostShader.cpp \
				$(GPUDIR)/Debugger/Breakpoints.cpp \
				$(GPUDIR)/Debugger/Stepping.cpp \
				$(GPUDIR)/GLES/DepalettizeShader.cpp \
				$(GPUDIR)/GLES/GLES_GPU.cpp \
				$(GPUDIR)/GLES/FragmentShaderGenerator.cpp \
				$(GPUDIR)/GLES/TransformPipeline.cpp \
				$(GPUDIR)/GLES/Framebuffer.cpp \
				$(NATIVEDIR)/base/backtrace.cpp \
				$(NATIVEDIR)/audio/mixer.cpp \
				$(GPUDIR)/GLES/ShaderManager.cpp \
				$(NATIVEDIR)/audio/wav_read.cpp \
				$(NATIVEDIR)/base/buffer.cpp \
				$(NATIVEDIR)/base/colorutil.cpp \
				$(NATIVEDIR)/base/display.cpp \
				$(GPUDIR)/GLES/Spline.cpp \
				$(NATIVEDIR)/base/error_context.cpp \
				$(NATIVEDIR)/base/fastlist_test.cpp \
				$(NATIVEDIR)/base/stringutil.cpp \
				$(GPUDIR)/GLES/StateMapping.cpp \
				$(NATIVEDIR)/base/timeutil.cpp \
				$(NATIVEDIR)/data/compression.cpp \
				$(GPUDIR)/GLES/StencilBuffer.cpp \
				$(NATIVEDIR)/ext/vjson/block_allocator.cpp \
				$(GPUDIR)/GLES/TextureCache.cpp \
				$(GPUDIR)/GLES/TextureScaler.cpp
CXXFILES += $(NATIVEDIR)/file/chunk_file.cpp \
				$(GPUDIR)/GLES/SoftwareTransform.cpp \
				$(NATIVEDIR)/file/dialog.cpp \
				$(NATIVEDIR)/file/easy_file.cpp \
				$(GPUDIR)/GLES/VertexDecoder.cpp \
				$(NATIVEDIR)/file/fd_util.cpp \
				$(NATIVEDIR)/file/file_util.cpp \
				$(NATIVEDIR)/file/ini_file.cpp \
				$(GPUDIR)/GLES/VertexShaderGenerator.cpp \
				$(GPUDIR)/GeDisasm.cpp \
				$(NATIVEDIR)/file/path.cpp \
				$(GPUDIR)/GPUCommon.cpp \
				$(GPUDIR)/GPUState.cpp \
				$(GPUDIR)/Math3D.cpp \
				$(NATIVEDIR)/file/zip_read.cpp \
				$(GPUDIR)/Null/NullGpu.cpp \
				$(GPUDIR)/Software/Clipper.cpp \
				$(GPUDIR)/Software/Lighting.cpp \
				$(GPUDIR)/Software/Rasterizer.cpp \
				$(NATIVEDIR)/gfx/gl_debug_log.cpp \
				$(NATIVEDIR)/gfx/gl_lost_manager.cpp \
				$(GPUDIR)/Software/SoftGpu.cpp \
				$(NATIVEDIR)/gfx/texture.cpp \
				$(NATIVEDIR)/gfx/texture_atlas.cpp \
				$(NATIVEDIR)/gfx/texture_gen.cpp \
				$(GPUDIR)/Software/TransformUnit.cpp \
				$(NATIVEDIR)/gfx_es2/draw_buffer.cpp \
				$(NATIVEDIR)/gfx_es2/draw_text.cpp \
				$(NATIVEDIR)/gfx_es2/fbo.cpp \
				$(NATIVEDIR)/gfx_es2/gl_state.cpp \
				$(NATIVEDIR)/gfx_es2/gpu_features.cpp \
				$(NATIVEDIR)/gfx_es2/glsl_program.cpp \
				$(NATIVEDIR)/gfx_es2/vertex_format.cpp \
				$(NATIVEDIR)/gfx_es2/gl3stub.cpp \
				$(NATIVEDIR)/i18n/i18n.cpp \
				$(NATIVEDIR)/image/zim_load.cpp \
				$(NATIVEDIR)/image/zim_save.cpp \
				$(NATIVEDIR)/image/png_load.cpp \
				$(NATIVEDIR)/input/gesture_detector.cpp \
				$(NATIVEDIR)/input/input_state.cpp \
				$(NATIVEDIR)/math/fast/fast_math.cpp \
				$(NATIVEDIR)/math/curves.cpp \
				$(NATIVEDIR)/math/expression_parser.cpp \
				$(NATIVEDIR)/math/lin/plane.cpp \
				$(NATIVEDIR)/math/lin/quat.cpp \
				$(NATIVEDIR)/math/lin/vec3.cpp \
				$(NATIVEDIR)/math/lin/matrix4x4.cpp \
				$(NATIVEDIR)/math/math_util.cpp \
				$(NATIVEDIR)/net/http_client.cpp \
				$(NATIVEDIR)/net/resolve.cpp \
				$(NATIVEDIR)/net/url.cpp \
				$(NATIVEDIR)/profiler/profiler.cpp \
				$(NATIVEDIR)/thread/prioritizedworkqueue.cpp \
				$(NATIVEDIR)/thread/threadutil.cpp \
				$(NATIVEDIR)/ui/screen.cpp \
				$(NATIVEDIR)/thread/threadpool.cpp \
				$(NATIVEDIR)/ui/ui.cpp \
				$(NATIVEDIR)/ui/ui_context.cpp \
				$(NATIVEDIR)/ui/ui_screen.cpp \
				$(NATIVEDIR)/ui/view.cpp \
				$(NATIVEDIR)/ui/viewgroup.cpp \
				$(NATIVEDIR)/ui/virtual_input.cpp \
				$(NATIVEDIR)/util/bits/bits.cpp \
				$(NATIVEDIR)/util/bits/varint.cpp \
				$(NATIVEDIR)/util/hash/hash.cpp \
				$(NATIVEDIR)/util/random/perlin.cpp \
				$(NATIVEDIR)/util/text/utf8.cpp \
				$(NATIVEDIR)/util/text/parsers.cpp \
				$(NATIVEDIR)/ext/jpge/jpgd.cpp \
				$(NATIVEDIR)/ext/jpge/jpge.cpp \
				$(COREDIR)/Config.cpp \
				$(COREDIR)/Core.cpp \
				$(COREDIR)/CoreTiming.cpp \
				$(COREDIR)/CwCheat.cpp \
				$(COREDIR)/HDRemaster.cpp \
				$(COREDIR)/Debugger/Breakpoints.cpp \
				$(COREDIR)/Debugger/SymbolMap.cpp \
				$(COREDIR)/Debugger/DisassemblyManager.cpp \
				$(COREDIR)/Dialog/PSPDialog.cpp \
				$(COREDIR)/Dialog/PSPGamedataInstallDialog.cpp \
				$(COREDIR)/Dialog/PSPMsgDialog.cpp \
				$(COREDIR)/Dialog/PSPNetconfDialog.cpp \
				$(COREDIR)/Dialog/PSPOskDialog.cpp \
				$(COREDIR)/Dialog/PSPPlaceholderDialog.cpp \
				$(COREDIR)/Dialog/PSPSaveDialog.cpp \
				$(COREDIR)/Dialog/PSPScreenshotDialog.cpp \
				$(COREDIR)/Dialog/SavedataParam.cpp \
				$(COREDIR)/ELF/ElfReader.cpp \
				$(COREDIR)/ELF/PBPReader.cpp \
				$(COREDIR)/ELF/PrxDecrypter.cpp \
				$(COREDIR)/ELF/ParamSFO.cpp \
				$(COREDIR)/FileSystems/tlzrc.cpp \
				$(COREDIR)/FileSystems/BlockDevices.cpp \
				$(COREDIR)/FileSystems/DirectoryFileSystem.cpp \
				$(COREDIR)/FileSystems/FileSystem.cpp \
				$(COREDIR)/FileSystems/ISOFileSystem.cpp \
				$(COREDIR)/FileSystems/MetaFileSystem.cpp \
				$(COREDIR)/FileSystems/VirtualDiscFileSystem.cpp \
				$(COREDIR)/Font/PGF.cpp \
				$(COREDIR)/HLE/HLE.cpp \
				$(COREDIR)/HLE/ReplaceTables.cpp \
				$(COREDIR)/HLE/HLEHelperThread.cpp \
				$(COREDIR)/HLE/HLETables.cpp \
				$(COREDIR)/HLE/__sceAudio.cpp \
				$(COREDIR)/HLE/sceAtrac.cpp \
				$(COREDIR)/HLE/sceAudio.cpp \
				$(COREDIR)/HLE/sceAudiocodec.cpp \
				$(COREDIR)/HLE/sceCcc.cpp \
				$(COREDIR)/HLE/sceChnnlsv.cpp \
				$(COREDIR)/HLE/sceCtrl.cpp \
				$(COREDIR)/HLE/sceDeflt.cpp \
				$(COREDIR)/HLE/sceDisplay.cpp \
				$(COREDIR)/HLE/sceDmac.cpp \
				$(COREDIR)/HLE/sceGameUpdate.cpp \
				$(COREDIR)/HLE/sceGe.cpp \
				$(COREDIR)/HLE/sceFont.cpp \
				$(COREDIR)/HLE/sceHeap.cpp \
				$(COREDIR)/HLE/sceHprm.cpp \
				$(COREDIR)/HLE/sceHttp.cpp \
				$(COREDIR)/HLE/sceImpose.cpp \
				$(COREDIR)/HLE/sceIo.cpp \
				$(COREDIR)/HLE/sceJpeg.cpp \
				$(COREDIR)/HLE/sceKernel.cpp \
				$(COREDIR)/HLE/sceKernelAlarm.cpp \
				$(COREDIR)/HLE/sceKernelEventFlag.cpp \
				$(COREDIR)/HLE/sceKernelInterrupt.cpp \
				$(COREDIR)/HLE/sceKernelMbx.cpp \
				$(COREDIR)/HLE/sceKernelMemory.cpp \
				$(COREDIR)/HLE/sceKernelModule.cpp \
				$(COREDIR)/HLE/sceKernelMsgPipe.cpp \
				$(COREDIR)/HLE/sceKernelMutex.cpp \
				$(COREDIR)/HLE/sceKernelSemaphore.cpp \
				$(COREDIR)/HLE/sceKernelThread.cpp \
				$(COREDIR)/HLE/sceKernelTime.cpp \
				$(COREDIR)/HLE/sceKernelVTimer.cpp \
				$(COREDIR)/HLE/sceMpeg.cpp \
				$(COREDIR)/HLE/sceNet.cpp \
				$(COREDIR)/HLE/sceNetAdhoc.cpp \
				$(COREDIR)/HLE/proAdhoc.cpp \
				$(COREDIR)/HLE/sceOpenPSID.cpp \
				$(COREDIR)/HLE/sceP3da.cpp \
				$(COREDIR)/HLE/sceMt19937.cpp \
				$(COREDIR)/HLE/sceMd5.cpp \
				$(COREDIR)/HLE/sceMp4.cpp \
				$(COREDIR)/HLE/sceMp3.cpp \
				$(COREDIR)/HLE/sceParseHttp.cpp \
				$(COREDIR)/HLE/sceParseUri.cpp \
				$(COREDIR)/HLE/scePower.cpp \
				$(COREDIR)/HLE/scePsmf.cpp \
				$(COREDIR)/HLE/sceRtc.cpp \
				$(COREDIR)/HLE/sceSas.cpp \
				$(COREDIR)/HLE/sceSsl.cpp \
				$(COREDIR)/HLE/sceUmd.cpp \
				$(COREDIR)/HLE/sceUsb.cpp \
				$(COREDIR)/HLE/sceUtility.cpp \
				$(COREDIR)/HLE/sceVaudio.cpp \
				$(COREDIR)/HLE/scePspNpDrm_user.cpp \
				$(COREDIR)/HLE/sceNp.cpp \
				$(COREDIR)/HLE/scePauth.cpp \
				$(COREDIR)/HW/SimpleAudioDec.cpp \
				$(COREDIR)/HW/AsyncIOManager.cpp \
				$(COREDIR)/HW/MediaEngine.cpp \
				$(COREDIR)/HW/MpegDemux.cpp \
				$(COREDIR)/HW/MemoryStick.cpp \
				$(COREDIR)/HW/SasAudio.cpp \
				$(COREDIR)/Host.cpp \
				$(COREDIR)/Loaders.cpp \
				$(COREDIR)/MIPS/JitCommon/JitCommon.cpp \
				$(COREDIR)/MIPS/JitCommon/JitBlockCache.cpp \
				$(COREDIR)/MIPS/MIPS.cpp \
				$(COREDIR)/MIPS/MIPSAnalyst.cpp \
				$(COREDIR)/MIPS/MIPSCodeUtils.cpp \
				$(COREDIR)/MIPS/MIPSDebugInterface.cpp \
				$(COREDIR)/MIPS/MIPSDis.cpp \
				$(COREDIR)/MIPS/MIPSDisVFPU.cpp \
				$(COREDIR)/MIPS/MIPSInt.cpp \
				$(COREDIR)/MIPS/MIPSIntVFPU.cpp \
				$(COREDIR)/MIPS/MIPSStackWalk.cpp \
				$(COREDIR)/MIPS/MIPSTables.cpp \
				$(COREDIR)/MIPS/MIPSVFPUUtils.cpp \
				$(COREDIR)/MIPS/MIPSAsmTables.cpp \
				$(COREDIR)/MIPS/MIPSAsm.cpp \
				$(COREDIR)/MemMap.cpp \
				$(COREDIR)/MemMapFunctions.cpp \
				$(COREDIR)/PSPLoaders.cpp \
				$(COREDIR)/PSPMixer.cpp \
				$(COREDIR)/Reporting.cpp \
				$(COREDIR)/SaveState.cpp \
				$(COREDIR)/System.cpp \
				$(COREDIR)/Util/GameManager.cpp \
				$(COREDIR)/Util/BlockAllocator.cpp \
				$(COREDIR)/Util/PPGeDraw.cpp \
				$(COREDIR)/Util/ppge_atlas.cpp \
				$(LIBRETRODIR)/libretro.cpp \
				$(LIBRETRODIR)/libretro_osm.cpp \
				$(COMMONDIR)/CPUDetect.cpp


### DYNAREC ###
ifdef WITH_DYNAREC
   DYNAFLAGS += -DDYNAREC
   ifeq ($(WITH_DYNAREC), arm)
      DYNAFLAGS += -DNEW_DYNAREC=3
		CXXFILES += $(COMMONDIR)/ArmEmitter.cpp \
						$(EXTDIR)/disarm.cpp
   else
	   CPUFLAGS := -msse -msse2 -D_M_X64
	   CXXFILES += $(COMMONDIR)/x64Analyzer.cpp \
						$(COMMONDIR)/x64Emitter.cpp \
						$(COREDIR)/MIPS/x86/CompReplace.cpp \
						$(COREDIR)/MIPS/x86/CompBranch.cpp \
						$(COREDIR)/MIPS/x86/Asm.cpp \
						$(COREDIR)/MIPS/x86/CompALU.cpp \
						$(COREDIR)/MIPS/x86/CompVFPU.cpp \
						$(COREDIR)/MIPS/x86/CompLoadStore.cpp \
						$(COREDIR)/MIPS/x86/CompFPU.cpp \
						$(COREDIR)/MIPS/x86/Jit.cpp \
						$(COREDIR)/MIPS/x86/JitSafeMem.cpp \
						$(COREDIR)/MIPS/x86/RegCache.cpp \
						$(COREDIR)/MIPS/x86/RegCacheFPU.cpp \
						$(GPUDIR)/GLES/VertexDecoderX86.cpp
		CFILES   += $(NATIVEDIR)/math/fast/fast_matrix_sse.c
   endif
endif

ifeq ($(PERF_TEST), 1)
COREFLAGS += -DPERF_TEST
endif

ifeq ($(HAVE_SHARED_CONTEXT), 1)
COREFLAGS += -DHAVE_SHARED_CONTEXT
endif

ifeq ($(SINGLE_THREAD), 1)
COREFLAGS += -DSINGLE_THREAD
endif

CFILES += $(LIBRETRO_SRC)

ifeq ($(GLES), 1)
GLFLAGS += -DGLES -DHAVE_OPENGLES2 -DDISABLE_3POINT
else
GLFLAGS += -DHAVE_OPENGL
endif

COREFLAGS += -D__LIBRETRO__ -DINLINE="inline" -DPPSSPP -DUSE_FFMPEG -DBAKE_IN_GIT

ifeq ($(DEBUG), 1)
   CPUOPTS += -O0 -g
   CPUOPTS += -DOPENGL_DEBUG
else
   CPUOPTS += -O2 -DNDEBUG
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
