// TODO(dan): remove these includes
#include <mmsystem.h>
#include <dsound.h>

// TODO(dan): remove when we replace wav testfiles with streaming audio
#include "ani_riff.h"

typedef HRESULT WINAPI DIRECTSOUNDCREATE(LPCGUID, LPDIRECTSOUND *, LPUNKNOWN);
typedef HRESULT WINAPI DIRECTSOUNDCAPTURECREATE(LPCGUID, LPDIRECTSOUNDCAPTURE *, LPUNKNOWN);
typedef HRESULT WINAPI DIRECTSOUNDFULLDUPLEXCREATE(LPCGUID, LPCGUID, LPCDSCBUFFERDESC, LPCDSBUFFERDESC, 
                                                   HWND, DWORD, LPDIRECTSOUNDFULLDUPLEX *, 
                                                   LPDIRECTSOUNDCAPTUREBUFFER *, LPDIRECTSOUNDBUFFER *, LPUNKNOWN);

static LPDIRECTSOUNDBUFFER global_sound_buffer;
static LPDIRECTSOUNDCAPTUREBUFFER global_capture_buffer;

static void win32_init_dsound(Win32Audio *audio, HWND window)
{
    HMODULE lib = LoadLibraryA("dsound.dll");

    if (lib)
    {
        DIRECTSOUNDFULLDUPLEXCREATE *duplex_create = (DIRECTSOUNDFULLDUPLEXCREATE *)GetProcAddress(lib, "DirectSoundFullDuplexCreate");
        LPDIRECTSOUNDFULLDUPLEX full_duplex;

        WAVEFORMATEX wave_format = {0};
        wave_format.wFormatTag      = WAVE_FORMAT_PCM;
        wave_format.nChannels       = audio->num_channels;
        wave_format.nSamplesPerSec  = audio->samples_per_sec;
        wave_format.wBitsPerSample  = audio->bits_per_sample;
        wave_format.nBlockAlign     = wave_format.nChannels * wave_format.wBitsPerSample / 8;
        wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;

        DSCEFFECTDESC effects[2] = {0};
        effects[0].dwSize            = sizeof(DSCEFFECTDESC);
        effects[0].dwFlags           = DSCFX_LOCSOFTWARE;
        effects[0].guidDSCFXClass    = GUID_DSCFX_CLASS_AEC;
        effects[0].guidDSCFXInstance = GUID_DSCFX_SYSTEM_AEC;

        effects[1].dwSize            = sizeof(DSCEFFECTDESC);
        effects[1].dwFlags           = DSCFX_LOCSOFTWARE;
        effects[1].guidDSCFXClass    = GUID_DSCFX_CLASS_NS;
        effects[1].guidDSCFXInstance = GUID_DSCFX_SYSTEM_NS;

        DSCBUFFERDESC capture_buffer_desc = {0};
        capture_buffer_desc.dwSize        = sizeof(capture_buffer_desc);
        capture_buffer_desc.dwFlags       = DSCBCAPS_CTRLFX;
        capture_buffer_desc.dwFXCount     = array_size(effects);
        capture_buffer_desc.lpDSCFXDesc   = effects;
        capture_buffer_desc.dwBufferBytes = audio->buffer_size;
        capture_buffer_desc.lpwfxFormat   = &wave_format;

        DSBUFFERDESC buffer_desc = {0};
        buffer_desc.dwSize        = sizeof(buffer_desc);
        buffer_desc.dwFlags       = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_LOCSOFTWARE;
        buffer_desc.dwBufferBytes = audio->buffer_size;
        buffer_desc.lpwfxFormat   = &wave_format;

        if (duplex_create && SUCCEEDED(duplex_create(0, // DirectSoundCaptureEnumerate GUID, or null for default capture device
                                                     0, // DirectSoundEnumerate GUID, or null for default render device
                                                     &capture_buffer_desc,
                                                     &buffer_desc,
                                                     window,
                                                     DSSCL_PRIORITY,
                                                     &full_duplex,
                                                     &global_capture_buffer,
                                                     &global_sound_buffer,
                                                     0)))
        {
            // NOTE(dan): capture/sound buffers created
        }
    }
}

static void win32_clear_sound_buffer(Win32Audio *audio)
{
    void *buffer1, *buffer2;
    ulong size1, size2;

    if (SUCCEEDED(global_sound_buffer->Lock(0, 0, &buffer1, &size1, &buffer2, &size2, DSBLOCK_ENTIREBUFFER)))
    {
        zero_size(buffer1, size1);
        zero_size(buffer2, size2);

        global_sound_buffer->Unlock(buffer1, size1, buffer2, size2);
    }

    ulong play_cursor, write_cursor;
    if (SUCCEEDED(global_sound_buffer->GetCurrentPosition(&play_cursor, &write_cursor)))
    {
        audio->samples_played = write_cursor / audio->block_size;
    }
}

static void win32_init_audio(Win32Audio *audio, HWND window)
{
    audio->block_size     = audio->num_channels * (audio->bits_per_sample / 8);
    audio->guard_samples  = audio->samples_per_sec * audio->block_size / UPDATE_HZ / 2;
    audio->samples_played = 0;
    audio->buffer_size    = audio->samples_per_sec * audio->block_size;
    audio->buffer         = win32_allocate(audio->buffer_size);

    win32_init_dsound(audio, window);
    win32_clear_sound_buffer(audio);
 
    global_sound_buffer->Play(0, 0, DSBPLAY_LOOPING);
    global_capture_buffer->Start(DSCBSTART_LOOPING);
}

static void win32_update_audio_buffer(Win32Audio *audio, ulong lock_offset, u32 write_size)
{
    void *buffer1, *buffer2;
    ulong size1, size2;

    if (SUCCEEDED(global_sound_buffer->Lock(lock_offset, write_size, &buffer1, &size1, &buffer2, &size2, 0)))
    {
        memcpy(buffer1, audio->buffer, size1);
        audio->samples_played += size1 / audio->block_size;

        if (size2)
        {
            memcpy(buffer2, (u8 *)audio->buffer + size1, size2);
            audio->samples_played += size2 / audio->block_size;
        }

        global_sound_buffer->Unlock(buffer1, size1, buffer2, size2);
    }
}

static void win32_update_audio(Win32State *state, Win32Audio *audio)
{
    // NOTE(dan): capture

    ulong read_cursor;
    if (SUCCEEDED(global_capture_buffer->GetCurrentPosition(0, &read_cursor)))
    {
        void *buffer1, *buffer2;
        ulong size1, size2;        
        long lock_size = read_cursor - audio->next_capture_offset;

        if (lock_size < 0)
        {
            lock_size += audio->buffer_size;
        }

        if (SUCCEEDED(global_capture_buffer->Lock(audio->next_capture_offset, lock_size, &buffer1, &size1, &buffer2, &size2, 0)))
        {
            state->record_audio(&state->permanent_memory, buffer1, size1, buffer2, size2);

            audio->next_capture_offset += size1 + size2;
            audio->next_capture_offset %= audio->buffer_size;

            assert(audio->next_capture_offset >= 0);
            assert(audio->next_capture_offset < audio->buffer_size);

            global_capture_buffer->Unlock(buffer1, size1, buffer2, size2);
        }
    }

    // NOTE(dan): playback
    ulong play_cursor, write_cursor;

    if (SUCCEEDED(global_sound_buffer->GetCurrentPosition(&play_cursor, &write_cursor)))
    {
        ulong lock_offset = (audio->samples_played * audio->block_size) % audio->buffer_size;
        ulong bytes_per_frame = (ulong)((f32)audio->samples_per_sec * (f32)audio->block_size * TIMESTEP_SEC);
        ulong frame_end_byte = play_cursor + bytes_per_frame;
        ulong safe_write_cursor = write_cursor;
        ulong target_cursor = 0;
        ulong write_size = 0;

        if (safe_write_cursor < play_cursor)
        {
            safe_write_cursor += audio->buffer_size;
        }

        safe_write_cursor += audio->guard_samples;

        if (safe_write_cursor < frame_end_byte)
        {
            target_cursor = frame_end_byte + bytes_per_frame;
        }
        else
        {
            target_cursor = write_cursor + bytes_per_frame + audio->guard_samples;
        }

        target_cursor = target_cursor % audio->buffer_size;
        
        if (lock_offset > target_cursor)
        {
            write_size = target_cursor + audio->buffer_size - lock_offset;
        }
        else
        {
            write_size = target_cursor - lock_offset;
        }

        u32 num_samples = write_size / audio->block_size;
        state->mix_audio(&state->permanent_memory, audio->num_channels, audio->bits_per_sample, audio->samples_per_sec,
                         audio->buffer, num_samples);
        win32_update_audio_buffer(audio, lock_offset, write_size);
    }
}
