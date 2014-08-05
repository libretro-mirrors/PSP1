#pragma once

#include "libretro.h"
#include "gfx/gl_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern GLuint libretro_framebuffer;
extern retro_hw_get_proc_address_t libretro_get_proc_address;

#ifdef __cplusplus
}
#endif
