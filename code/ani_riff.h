#ifndef ANI_RIFF_H

#define FOURCC(a, b, c, d) (((u32)(a) << 0) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

enum
{
    RiffChunk_ID_Riff = FOURCC('R', 'I', 'F', 'F'),
    RiffChunk_ID_Wave = FOURCC('W', 'A', 'V', 'E'),
    RiffChunk_ID_fmt  = FOURCC('f', 'm', 't', ' '),
    RiffChunk_ID_data = FOURCC('d', 'a', 't', 'a'),
};

#pragma pack(push, 1)
typedef struct
{
    u32 id;
    u32 size;
} RiffChunk;

typedef struct
{
    RiffChunk riff;
    u32 format;
} RiffHeader;

typedef struct
{
    u16 format_tag;
    u16 num_channels;
    u32 samples_per_sec;
    u32 avg_bytes_per_sec;
    u16 block_align;
} WaveFormatChunk;

typedef struct
{
    u16 bits_per_sample;
} WaveFormatPCMChunk;
#pragma pack(pop)

typedef struct
{
    u8 *at;
    u8 *end;
} RiffIterator;

inline RiffIterator get_riff_iterator(RiffHeader *header)
{
    RiffIterator iterator = {0};
    iterator.at = (u8 *)(header + 1);
    iterator.end = iterator.at + header->riff.size - 4;
    return iterator;
}

inline b32 riff_iterator_valid(RiffIterator iterator)
{
    b32 result = iterator.at < iterator.end;
    return result;
}

inline RiffIterator next_riff_chunk(RiffIterator it)
{
    RiffChunk *chunk = (RiffChunk *)it.at;
    u32 chunk_size = (chunk->size + 1) & ~1;

    it.at += sizeof(RiffChunk) + chunk_size;
    return it;
}

typedef struct 
{
    u16 num_channels;
    u16 bits_per_sample;
    u32 samples_per_sec;
    u32 num_samples;
    void *samples;
} ParsedWav;

static ParsedWav parse_wav(void *memory, usize size)
{
    ParsedWav wav = {0};
    RiffHeader *header = (RiffHeader *)memory;

    if (header->riff.id == RiffChunk_ID_Riff && header->format == RiffChunk_ID_Wave)
    {
        RiffIterator it;
        u32 total_size = 0;
        u32 sample_size = 0;

        for (it = get_riff_iterator(header); riff_iterator_valid(it); it = next_riff_chunk(it))
        {
            RiffChunk *chunk = (RiffChunk *)it.at;

            switch (chunk->id)
            {
                case RiffChunk_ID_fmt:
                {
                    WaveFormatChunk *fmt = (WaveFormatChunk *)(it.at + sizeof(RiffChunk));
                    wav.num_channels = fmt->num_channels;
                    wav.samples_per_sec = fmt->samples_per_sec;

                    if (fmt->format_tag == WAVE_FORMAT_PCM)
                    {
                        WaveFormatPCMChunk *pcm = (WaveFormatPCMChunk *)((u8 *)(fmt + 1));
                        wav.bits_per_sample = pcm->bits_per_sample;
                    }
                } break;

                case RiffChunk_ID_data:
                {
                    wav.samples = it.at + sizeof(RiffChunk);
                    total_size = chunk->size;
                } break;
            }
        }

        sample_size = wav.num_channels * (wav.bits_per_sample / 8);

        if (sample_size > 0)
        {
            wav.num_samples = total_size / sample_size;
        }
    }

    return wav;
}

#define ANI_RIFF_H
#endif
