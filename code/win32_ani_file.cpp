
typedef struct 
{
    u32 size;
    void *contents;
} LoadedFile;

static LoadedFile win32_load_file(char *filename)
{
    LoadedFile file = {0};
    HANDLE handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

    if (handle != INVALID_HANDLE_VALUE)
    {
        i64 size64;

        if (GetFileSizeEx(handle, (LARGE_INTEGER *)&size64))
        {
            u32 size32 = win32_truncate_uint64(size64);
            file.contents = win32_allocate(size32);

            if (file.contents)
            {
                ulong bytes_read;

                if (ReadFile(handle, file.contents, size32, &bytes_read, 0) && (size32 == bytes_read))
                {
                    file.size = size32;
                }
                else
                {
                    win32_free(file.contents);
                    file.contents = 0;
                }
            }
        }

        CloseHandle(handle);
    }

    return file;
}
