#ifndef ANI_PLATFORM_H

#if INTERNAL_BUILD
    #define assert(e) if (!(e)) { *(i32 *)0 = 0; }
#else
    #define assert(e)
#endif

#define invalid_codepath        assert(!"invalid codepath")
#define invalid_default_case    default: { invalid_codepath; } break

#define array_size(a) (sizeof(a) / sizeof((a)[0]))
#define offset_of(type, element) ((usize)&(((type *)0)->element))

#define KB  (1024LL)
#define MB  (1024LL * KB)
#define GB  (1024LL * MB)
#define TB  (1024LL * GB)

#include "ani_types.h"
#include "ani_memory.h"
#include "ani_input.h"

#define UPDATE_AND_RENDER_PROC(name) void name(Memchunk *memchunk, Input *input, i32 window_width, i32 window_height)
typedef UPDATE_AND_RENDER_PROC(UpdateAndRenderProc);

#define RECORD_AUDIO_PROC(name) void name(Memchunk *memchunk, void *buffer, u32 size)
typedef RECORD_AUDIO_PROC(RecordAudioProc);

#define MIX_AUDIO_PROC(name) void name(Memchunk *memchunk, u16 num_channels, u16 bits_per_sample, u32 samples_per_sec, void *buffer, u32 num_samples)
typedef MIX_AUDIO_PROC(MixAudioProc);

#define ANI_PLATFORM_H
#endif
