static ProgramState *get_or_create_program_state(Memchunk *memchunk);

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
    record->buffer      = push_size(&state->record_memory, record->buffer_size, align_no_clear(16));

    record->write_cursor = 0;
    record->read_cursor  = 0;

    record->volume[0] = record->volume[1] = 0.5f;

    record->want_playback = true;
}

static RECORD_AUDIO_PROC(record_audio)
{
    ProgramState *program = get_or_create_program_state(&memory->memchunk);
    AudioState *audio = &program->audio_state;

    if (!audio->recording_initialized)
    {
        sub_memchunk(&audio->record_memory, &memory->memchunk, 5*MB);

        audio->local_record = allocate_audio_record(audio);
        init_recording(audio, audio->local_record, 2, 16, 48000);

        audio->recording_initialized = true;
    }

    AudioRecord *record = audio->local_record;
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
    ProgramState *program = get_or_create_program_state(&memory->memchunk);
    AudioState *audio = &program->audio_state;

    if (!audio->mixer_initialized)
    {
        sub_memchunk(&audio->mixer_memory, &memory->memchunk, 1*MB);

        audio->master_volume[0] = 1.0f;
        audio->master_volume[1] = 1.0f;

        audio->mixer_initialized = true;
    }

    TempMemchunk mixer_memory = begin_temp_memchunk(&audio->mixer_memory);

    #if 1 
    // NOTE(dan): SIMD version of the mixer

    assert(output_num_samples % 4 == 0);
    u32 sample_chunk_count = output_num_samples / 4;

    __m128 *mixer_channel0 = push_array(&audio->mixer_memory, sample_chunk_count, __m128, align_no_clear(16));
    __m128 *mixer_channel1 = push_array(&audio->mixer_memory, sample_chunk_count, __m128, align_no_clear(16));

    // NOTE(dan): clear audio buffer
    {
        __m128 *dest0 = mixer_channel0;
        __m128 *dest1 = mixer_channel1;
        __m128 zero = _mm_setzero_ps();

        for (u32 sample_index = 0; sample_index < sample_chunk_count; ++sample_index)
        {
            _mm_store_ps((f32 *)dest0++, zero);
            _mm_store_ps((f32 *)dest1++, zero);
        }
    }

    // NOTE(dan): mixer main loop
    {       
        __m128 master_volume0 = _mm_set1_ps(audio->master_volume[0]);
        __m128 master_volume1 = _mm_set1_ps(audio->master_volume[1]);

        for (AudioRecord *record = audio->first_record; record; record = record->next)
        {
            if (record == audio->local_record && !program->playback_mic)
            {
                continue;
            }

            if (record->want_playback)
            {
                u32 bytes_per_sample = record->num_channels * (record->bits_per_sample / 8);
                u32 record_size = (record->write_cursor - record->read_cursor) % record->buffer_size;
                u32 record_samples = record_size / bytes_per_sample;

                if (record_samples > output_num_samples)
                {
                    record_samples = output_num_samples;
                }

                i32 *source = (i32 *)record->buffer;
                u32 read_byte = record->read_cursor / bytes_per_sample;
                u32 buffer_bytes = (u32)record->buffer_size / bytes_per_sample;

                __m128 *dest0 = mixer_channel0;
                __m128 *dest1 = mixer_channel1;
             
                __m128 volume0 = _mm_set1_ps(record->volume[0]);
                __m128 volume1 = _mm_set1_ps(record->volume[1]);

                volume0 = _mm_mul_ps(master_volume0, volume0);
                volume1 = _mm_mul_ps(master_volume1, volume1);

                for (u32 sample_chunk_index = 0; sample_chunk_index < record_samples/4; ++sample_chunk_index)
                {
                    u32 source_index = (read_byte + sample_chunk_index * 4) % buffer_bytes;
                    assert(source_index < buffer_bytes);
                    
                    // NOTE(dan): load the samples as packed pairs of 16bit ints
                    __m128i sample_x4 = _mm_load_si128((__m128i *)(source + source_index));

                    // NOTE(dan): extract packed 16 bit pairs to separate 32bit packed floats
                    __m128 c0 = _mm_cvtepi32_ps(_mm_srai_epi32(sample_x4, 16));
                    __m128 c1 = _mm_cvtepi32_ps(_mm_srai_epi32(_mm_slli_epi32(sample_x4, 16), 16));

                    // NOTE(dan): load the mixer buffer values in
                    __m128 d0 = _mm_load_ps((f32 *)dest0);
                    __m128 d1 = _mm_load_ps((f32 *)dest1);

                    // NOTE(dan): apply the volume
                    d0 = _mm_add_ps(d0, _mm_mul_ps(volume0, c0));
                    d1 = _mm_add_ps(d1, _mm_mul_ps(volume1, c1));

                    // NOTE(dan): store back the values into mixer buffer
                    _mm_store_ps((f32 *)dest0++, d0);
                    _mm_store_ps((f32 *)dest1++, d1);
                }

                record->read_cursor += record_samples*4;
                record->read_cursor %= record->buffer_size;
            }
        }
    }

    // NOTE(dan): store the mixed result
    {
        __m128 *src0 = mixer_channel0;
        __m128 *src1 = mixer_channel1;
        __m128i *dest = (__m128i *)output_buffer;

        for (u32 sample_index = 0; sample_index < sample_chunk_count; ++sample_index)
        {
            // NOTE(dan): load channel values in
            __m128 s0 = _mm_load_ps((f32 *)src0++);
            __m128 s1 = _mm_load_ps((f32 *)src1++);

            // NOTE(dan): f32 to i32
            __m128i l = _mm_cvtps_epi32(s0);
            __m128i r = _mm_cvtps_epi32(s1);

            // NOTE(dan): unpack and interleave the low/high half i32 values
            __m128i lr0 = _mm_unpacklo_epi32(l, r);
            __m128i lr1 = _mm_unpackhi_epi32(l, r);

            // NOTE(dan): store the values as L0R0L1R1
            *dest++ = _mm_packs_epi32(lr0, lr1);
        }
    }

    #else

    // NOTE(dan): scalar version of the sound mixer

    f32 *mixer_channel0 = push_array(&state->mixer_memory, num_samples, f32, align_no_clear(16));
    f32 *mixer_channel1 = push_array(&state->mixer_memory, num_samples, f32, align_no_clear(16));

    // NOTE(dan): clear audio buffer
    {
        f32 *dest0 = mixer_channel0;
        f32 *dest1 = mixer_channel1;

        for (u32 sample_index = 0; sample_index < num_samples; ++sample_index)
        {
            *dest0++ = 0;
            *dest1++ = 0;
        }
    }

    for (AudioRecord *record = state->first_record; record; record = record->next)
    {
        if (record == state->local_record && !permanent_state->playback_mic)
        {
            continue;
        }

        if (record->want_playback)
        {
            f32 *dest0 = channel0;
            f32 master_volume0 = state->master_volume[0];
            f32 volume0 = record->volume[0];
            
            f32 *dest1 = channel1;
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

                i32 *sample = (i32 *)record->buffer + index;
                i16 c0 = *(i16 *)source;
                i16 c1 = *((i16 *)source + 1);

                *dest0++ += volume0 * c0;
                *dest1++ += volume1 * c1;      
            }

            record->read_cursor += record_samples*4;
            record->read_cursor %= record->buffer_size;
        }
    }

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
    #endif

    end_temp_memchunk(mixer_memory);
}
