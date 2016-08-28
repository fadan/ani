#ifndef ANI_H

#include "ani_math.h"

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

    struct AudioStream *next;
};

struct AudioRecord
{
    b32 streaming;

    u16 num_channels;
    u16 bits_per_sample;
    u32 samples_per_sec;

    u32 write_cursor;
    u32 read_cursor;
    usize buffer_size;
    void *buffer;
};

struct AudioState
{
    b32 recording_initialized;
    b32 playback_initialized;

    AudioStream *music;
    Memchunk record_memory;
    Memchunk mixer_memory;

    AudioRecord record;
    
    AudioStream *first;
    AudioStream *first_free;
};

struct PermanentState
{
    AudioState audio_state;

    b32 initialized;
};

#define ANI_H
#endif
