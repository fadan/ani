#ifndef ANI_H

#include <intrin.h>
#include "ani_math.h"

#if 0
struct AudioStream
{
    u16 num_channels;
    u16 bits_per_sample;
    u32 samples_per_sec;

    u32 samples_played;
    u32 num_samples;
    void *buffer;

    // TODO(dan): samples_played should be in bytes
    // TODO(dan): bytes_per_second?

    AudioStream *next;
};
#endif

struct AudioRecord
{
    b32 want_playback;

    u16 num_channels;
    u16 bits_per_sample;
    u32 samples_per_sec;
    
    usize buffer_size;
    void *buffer;
    u32 write_cursor;
    u32 read_cursor;

    f32 volume[2];

    AudioRecord *next;
};

struct AudioState
{
    b32 recording_initialized;
    b32 mixer_initialized;

    Memchunk record_memory;
    Memchunk mixer_memory;

    AudioRecord *local_record;

    AudioRecord *first_record;
    AudioRecord *first_free_record;

    f32 master_volume[2];

    #if 0    
    AudioStream *music;
    AudioStream *first;
    AudioStream *first_free;
    #endif
};

struct ProgramState
{
    AudioState audio_state;
    NetworkState network_state;

    bool playback_mic;

    b32 initialized;
};

static Platform platform;

#define ANI_H
#endif
