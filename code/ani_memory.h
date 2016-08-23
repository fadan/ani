#ifndef ANI_MEMORY_H

#define zero_struct(instance)   zero_size(&(instance), sizeof(instance))
#define zero_array(base, count) zero_size(base, (count) * sizeof((base)[0]))

inline void zero_size(void *dest, usize size)
{
    // TODO(dan): optimize
    u8 *byte = (u8 *)dest;

    while (size--)
    {
        *byte++ = 0;
    }
}

#define copy_struct(source, dest)       copy_size(&(source), dest, sizeof(source))
#define copy_array(source, dest, count) copy_size(&(source), dest, count * sizeof(source))

inline void *copy_size(void *src, void *dest, usize size)
{
    // TODO(dan): optimize
    u8 *src_byte = (u8 *)src;
    u8 *dest_byte = (u8 *)dest;

    while (size--)
    {
        *dest_byte++ = *src_byte++;
    }
    return dest;
}

struct Memchunk
{
    usize size;
    usize used;
    u32 temp_subchunks;
    void *base;
};

struct TempMemchunk
{
    Memchunk *memchunk;
    usize used;
};

struct MemchunkPushParams
{
    b32 clear_to_zero;
    u32 alignment;
};

inline MemchunkPushParams def_memchunk_params()
{
    MemchunkPushParams params;
    params.clear_to_zero = true;
    params.alignment     = 4;
    return params;
}

inline MemchunkPushParams align_no_clear(u32 alignment)
{
    MemchunkPushParams params = def_memchunk_params();
    params.clear_to_zero = false;
    params.alignment     = alignment;
    return params;
}

inline MemchunkPushParams align_clear(u32 alignment)
{
    MemchunkPushParams params = def_memchunk_params();
    params.clear_to_zero = true;
    params.alignment     = alignment;
    return params;
}

inline MemchunkPushParams no_clear()
{
    MemchunkPushParams params = def_memchunk_params();
    params.clear_to_zero = false;
    return params;
}

inline usize get_memchunk_offset(Memchunk *memchunk, usize alignment)
{
    usize offset = 0;
    usize result = (usize)memchunk->base + memchunk->used;
    usize mask = alignment - 1;
    if (result & mask)
    {
        offset = alignment - (result & mask);
    }
    return offset;
}

inline usize get_memchunk_unused_size(Memchunk *memchunk, MemchunkPushParams params = def_memchunk_params())
{
    usize offset = get_memchunk_offset(memchunk, params.alignment);
    usize unused = memchunk->size - (memchunk->used + offset);
    return unused;
}

inline usize get_memchunk_effective_size(Memchunk *memchunk, usize size, MemchunkPushParams params = def_memchunk_params())
{
    size += get_memchunk_offset(memchunk, params.alignment);
    return size;
}

inline b32 memchunk_has_room(Memchunk *memchunk, usize size, MemchunkPushParams params = def_memchunk_params())
{
    usize effective_size = get_memchunk_effective_size(memchunk, size, params);
    b32 fit = ((memchunk->used + effective_size) <= memchunk->size);
    return fit;
}

#define push_struct(memchunk, type, ...)        (type *)push_size(memchunk, sizeof(type), ## __VA_ARGS__)
#define push_array(memchunk, count, type, ...)  (type *)push_size(memchunk, (count) * sizeof(type), ## __VA_ARGS__)
#define push_copy(memchunk, size, source, ...)  copy_size(source, push_size(memchunk, size, ## __VA_ARGS__), size)

inline void *push_size(Memchunk *memchunk, usize size, MemchunkPushParams params = def_memchunk_params())
{
    usize effective_size = get_memchunk_effective_size(memchunk, size, params);
    usize offset = get_memchunk_offset(memchunk, params.alignment);

    assert((memchunk->used + effective_size) <= memchunk->size);
    assert(effective_size >= size);

    void *result = (u8 *)memchunk->base + memchunk->used + offset;
    memchunk->used += effective_size;
    if (params.clear_to_zero)
    {
        zero_size(result, size);
    }
    return result;
}

inline void init_memchunk(Memchunk *memchunk, void *base, usize size)
{
    memchunk->base = base;
    memchunk->size = size;
    memchunk->used = 0;
    memchunk->temp_subchunks = 0;
}

inline void clear_memchunk(Memchunk *memchunk)
{
    init_memchunk(memchunk, memchunk->base, memchunk->size);
}

inline void sub_memchunk(Memchunk *result, Memchunk *memchunk, usize size, MemchunkPushParams params = def_memchunk_params())
{
    void *base = push_size(memchunk, size, params);
    init_memchunk(result, base, size);
}

inline void check_memchunk(Memchunk *memchunk)
{
    assert(memchunk->temp_subchunks == 0);
}

inline TempMemchunk begin_temp_memchunk(Memchunk *memchunk)
{
    TempMemchunk temp_memchunk;
    temp_memchunk.memchunk = memchunk;
    temp_memchunk.used     = memchunk->used;
    ++memchunk->temp_subchunks;
    return temp_memchunk;
}

inline void end_temp_memchunk(TempMemchunk temp_memchunk)
{
    Memchunk *memchunk = temp_memchunk.memchunk;
    assert(memchunk->used >= temp_memchunk.used);
    
    memchunk->used = temp_memchunk.used;
    assert(memchunk->temp_subchunks > 0);
    --memchunk->temp_subchunks;
}

#define ANI_MEMORY_H
#endif
