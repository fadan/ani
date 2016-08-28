#include "ani_ui.cpp"

inline PermanentState *get_or_create_permanent_state(Memchunk *memchunk)
{
    if (!memchunk->used)
    {
        assert(memchunk->size >= sizeof(PermanentState));
        push_struct(memchunk, PermanentState);
    }

    PermanentState *permanent_state = (PermanentState *)memchunk->base;
    return permanent_state;
}

#if 0
static AudioStream *play_audio(AudioState *state, u16 num_channels, u16 bits_per_sample, u32 samples_per_sec, 
                               void *buffer, u32 num_samples)
{
    if (!state->first_free)
    {
        state->first_free = push_struct(&state->mixer_memory, AudioStream);
        state->first_free->next = 0;
    }

    AudioStream *stream = state->first_free;
    state->first_free = stream->next;
    
    stream->num_channels = num_channels;
    stream->bits_per_sample = bits_per_sample;
    stream->samples_per_sec = samples_per_sec;
    stream->buffer = buffer;
    stream->samples_played = 0;
    stream->num_samples = num_samples;

    stream->next = state->first;
    state->first = stream;

    return stream;
}
#endif

static AudioRecord *allocate_audio_record(AudioState *state)
{    
    if (!state->first_free_record)
    {
        state->first_free_record = push_struct(&state->record_memory, AudioRecord);
        state->first_free_record->next = 0;
    }

    AudioRecord *result = state->first_free_record;
    state->first_free_record = result->next;

    result->next = state->first_record;
    state->first_record = result;

    return result;
}

static void init_recording(AudioState *state, AudioRecord *record, u16 num_channels, u16 bits_per_sample, u32 samples_per_sec)
{
    record->num_channels    = num_channels;
    record->bits_per_sample = bits_per_sample;
    record->samples_per_sec = samples_per_sec;

    record->buffer_size = record->samples_per_sec * record->num_channels * (record->bits_per_sample / 8);
    record->buffer      = push_size(&state->record_memory, record->buffer_size);

    record->write_cursor = 0;
    record->read_cursor  = 0;

    record->volume[0] = record->volume[1] = 0.5f;

    record->want_playback = true;
}

static UPDATE_AND_RENDER_PROC(update_and_render)
{
    PermanentState *permanent_state = get_or_create_permanent_state(memchunk);
    if (!permanent_state->initialized)
    {
        init_ui();

        permanent_state->initialized = true;
    }
    
    begin_ui(input, window_width, window_height);
    {
        ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiSetCond_FirstUseEver);

        static bool open = true;
        ImGui::Begin("audio", &open, 0);

        ImGui::End();
    }
    end_ui();
}

static RECORD_AUDIO_PROC(record_audio)
{
    PermanentState *permanent_state = get_or_create_permanent_state(memchunk);
    AudioState *audio_state = &permanent_state->audio_state;

    if (!audio_state->recording_initialized)
    {
        sub_memchunk(&audio_state->record_memory, memchunk, 5*MB);

        audio_state->local_record = allocate_audio_record(audio_state);
        init_recording(audio_state, audio_state->local_record, 2, 16, 48000);

        audio_state->recording_initialized = true;
    }

    AudioRecord *record = audio_state->local_record;
    usize remaining = record->buffer_size - record->write_cursor;

    if (size > remaining)
    {
        u8 *src1 = (u8 *)buffer;
        u8 *dest1 = (u8 *)record->buffer + record->write_cursor;
        usize src1_size = remaining;

        u8 *src2 = (u8 *)buffer + src1_size;
        u8 *dest2 = (u8 *)record->buffer;
        usize src2_size = size - src1_size;

        memcpy(dest1, src1, src1_size);
        memcpy(dest2, src2, src2_size);

        record->write_cursor = (u32)src2_size;
    }
    else
    {
        u8 *src = (u8 *)buffer;
        u8 *dest = (u8 *)record->buffer + record->write_cursor;

        memcpy(dest, src, size);

        record->write_cursor += size;
    }

    record->write_cursor %= record->buffer_size;
}

static MIX_AUDIO_PROC(mix_audio)
{
    PermanentState *permanent_state = get_or_create_permanent_state(memchunk);
    AudioState *state = &permanent_state->audio_state;

    if (!state->playback_initialized)
    {
        sub_memchunk(&state->mixer_memory, memchunk, 1*MB);

        state->master_volume[0] = state->master_volume[1] = 1.0f;

        // NOTE(dan): enable this to test playing wav files
        #if 0
        LoadedFile file = win32_load_file("w:\\ani\\data\\test.wav");
        ParsedWav wav = parse_wav(file.contents, file.size);

        state->music = play_audio(state, wav.num_channels, wav.bits_per_sample, wav.samples_per_sec, 
                                        wav.samples, wav.num_samples);
        #endif
        state->playback_initialized = true;
    }

    // TODO(dan): SIMD this
    TempMemchunk mixer_memory = begin_temp_memchunk(&state->mixer_memory);
    
    f32 *channel0 = push_array(&state->mixer_memory, num_samples, f32, align_no_clear(16));
    f32 *channel1 = push_array(&state->mixer_memory, num_samples, f32, align_no_clear(16));

    // NOTE(dan): clear audio buffer
    {
        f32 *dest0 = channel0;
        f32 *dest1 = channel1;

        for (u32 sample_index = 0; sample_index < num_samples; ++sample_index)
        {
            *dest0++ = 0;
            *dest1++ = 0;
        }
    }

    for (AudioRecord *record = state->first_record; record; record = record->next)
    {
        f32 *dest0 = channel0;
        f32 *dest1 = channel1;

        //if (record != state->local_record && record->want_playback)
        if (record->want_playback)
        {
            f32 master_volume0 = state->master_volume[0];
            f32 volume0 = record->volume[0];
            
            f32 master_volume1 = state->master_volume[1];
            f32 volume1 = record->volume[1];

            u32 record_size = (record->write_cursor - record->read_cursor) % record->buffer_size;
            u32 record_samples = record_size / (record->num_channels * (record->bits_per_sample / 8));

            if (record_samples > num_samples)
            {
                record_samples = num_samples;
            }

            for (u32 sample_index = 0; sample_index < record_samples; ++sample_index)
            {
                u32 index = (record->read_cursor/4 + sample_index) % (record->buffer_size/4);
                assert(index < record->buffer_size/4);

                i32 *source = (i32 *)record->buffer + index;
                i16 sample0 = *(i16 *)source;
                i16 sample1 = *((i16 *)source + 1);

                *dest0++ += volume0*sample0;
                *dest1++ += volume1*sample1;
                
                record->read_cursor %= record->buffer_size;
            }

            record->read_cursor += record_samples*4;
        }
    }

    #if 0
    AudioRecord *record = &state->record;
    if (record->streaming)
    {
        f32 *dest0 = channel0;
        f32 *dest1 = channel1;

        u32 record_size = (record->write_cursor - record->read_cursor) % record->buffer_size;
        u32 record_samples = record_size / (record->num_channels * (record->bits_per_sample / 8));

        if (record_samples > num_samples)
        {
            record_samples = num_samples;
        }

        for (u32 sample_index = 0; sample_index < record_samples; ++sample_index)
        {
            u32 index = (record->read_cursor/4 + sample_index) % (record->buffer_size/4);
            assert(index < record->buffer_size/4);

            i32 *source = (i32 *)record->buffer + index;
            i16 *sample = (i16 *)source;

            *dest0++ = *sample++;
            *dest1++ = *sample++;
            
            record->read_cursor %= record->buffer_size;
        }

        record->read_cursor += record_samples*4;
    }
    #endif

    // NOTE(dan): enable this to test playing wav files
    #if 0
    for (AudioStream **stream_ptr = &state->first; *stream_ptr; )
    {
        AudioStream *stream = *stream_ptr;

        f32 *dest0 = channel0;
        f32 *dest1 = channel1;

        u32 samples_to_mix = num_samples;
        u32 samples_remaining = stream->num_samples - stream->samples_played;

        if (samples_to_mix > samples_remaining)
        {
            samples_to_mix = samples_remaining;
        }

        i16 *source = (i16 *)stream->buffer + (stream->samples_played * (stream->bits_per_sample / 8));

        for (u32 sample_index = stream->samples_played; sample_index < (stream->samples_played + samples_to_mix); ++sample_index)
        {
            *dest0++ = *source++;
            *dest1++ = *source++;
        }

        stream->samples_played += samples_to_mix;

        if (stream->samples_played >= stream->num_samples)
        {
            *stream_ptr = stream->next;
            stream->next = state->first_free;
            state->first_free = stream;
        }
        else
        {
            stream_ptr = &stream->next;
        }
    }
    #endif

    // NOTE(dan): fill the audio buffer with the result
    {
        f32 *source0 = channel0;
        f32 *source1 = channel1;
        i16 *dest = (i16 *)buffer;

        for (u32 sample_index = 0; sample_index < num_samples; ++sample_index)
        {
            *dest++ = (i16)(*source0++ + 0.5f);
            *dest++ = (i16)(*source1++ + 0.5f);
        }
    }

    end_temp_memchunk(mixer_memory);
    clear_memchunk(&state->record_memory);
}
