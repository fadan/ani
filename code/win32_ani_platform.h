#ifndef WIN32_ANI_PLATFORM_H

// TODO(dan): remove windows.h?
#define WIN32_LEAN_AND_MEAN
#define INITGUID
#include "windows.h"

struct Win32Window
{
    char    *title;
    i32     width;
    i32     height;
    WNDPROC wndproc;
    HWND    wnd;
    HDC     dc;
    HGLRC   rc;
    b32     context_initialized;
};

struct Win32Audio
{ 
    u16 num_channels;
    u16 bits_per_sample;
    u32 samples_per_sec;

    u32 block_size;
    u32 guard_samples;
    u32 samples_played;
    ulong buffer_size;
    void *buffer;

    u32 next_capture_offset;
};

struct Win32State
{
    b32 initialized;

    Memchunk platform_memory;
    Memchunk program_memory;

    MixAudioProc        *mix_audio;
    RecordAudioProc     *record_audio;
    UpdateAndRenderProc *update_and_render;
};

#define win32_allocate_struct(type, ...)        (type *)win32_allocate(sizeof(type), ## __VA_ARGS__)
#define win32_allocate_array(count, type, ...)  (type *)win32_allocate((count) * sizeof(type), ## __VA_ARGS__)

inline void *win32_allocate(usize size, void *base_address = 0)
{
    void *result = VirtualAlloc(base_address, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    return result;
}

inline void win32_free(void *memory)
{
    VirtualFree(memory, 0, MEM_RELEASE);
}

inline f32 win32_get_time()
{
    static i64 start = 0;
    static i64 frequency = 0;
    f32 time = 0.0;

    if (!start)
    {
        QueryPerformanceCounter((LARGE_INTEGER *)&start);
        QueryPerformanceFrequency((LARGE_INTEGER *)&frequency);
    }
    else
    {
        i64 counter = 0;
        QueryPerformanceCounter((LARGE_INTEGER *)&counter);
        time = (f32)((counter - start) / (f64)frequency);
    }

    return time;
}

inline u32 win32_truncate_uint64(u64 value)
{
    assert(value <= 0xFFFFFFFF);

    u32 truncated = (u32)value;
    return truncated;
}

#include "win32_ani_gl.h"
#include "ani.h"

#include "win32_ani_file.cpp"
#include "win32_ani_audio.cpp"
#include "win32_ani_sockets.cpp"
#include "ani_net.cpp"
#include "ani.cpp"

#define WIN32_ANI_PLATFORM_H
#endif
