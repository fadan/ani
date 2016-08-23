#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#include "imgui.h"

#include "imgui_demo.cpp"
#include "imgui_draw.cpp"
#include "imgui.cpp"

#define SHADER_VERSION "#version 330\n"

static char *vertex_shader
{
    SHADER_VERSION

    "uniform mat4 proj_mat;\n"
    "in vec2 pos;\n"
    "in vec2 uv;\n"
    "in vec4 color;\n"
    "out vec2 frag_uv;\n"
    "out vec4 frag_color;\n"

    "void main()\n"
    "{\n"
    "   frag_uv = uv;\n"
    "   frag_color = color;\n"
    "   gl_Position = proj_mat * vec4(pos.xy, 0, 1);\n"
    "}\n"
};

static char *fragment_shader
{
    SHADER_VERSION

    "uniform sampler2D tex;\n"
    "in vec2 frag_uv;\n"
    "in vec4 frag_color;\n"
    "out vec4 out_color;\n"

    "void main()\n"
    "{\n"
    "   out_color = frag_color * texture(tex, frag_uv.st);\n"
    "}\n"
};

enum
{
    attrib_pos,
    attrib_uv,
    attrib_color,

    attrib_count,
};

enum
{
    uniform_tex,
    uniform_proj_mat,

    uniform_count,
};

GLint attribs[attrib_count];
GLint uniforms[uniform_count];

GLuint program;

GLuint vbo;
GLuint vao;
GLuint elements;
GLuint font_texture;

inline ImGuiIO *get_imgui_io()
{
    return &GImGui->IO;
}

inline void check_bindings(GLint *buffer, GLint num_elements)
{
    for (GLint element_index = 0; element_index < num_elements; ++element_index)
    {
        assert(buffer[element_index] != -1);
    }
}

static void init_imgui()
{
    ImGuiIO *io = get_imgui_io();
    u8 *pixels;
    i32 width, height;    
    
    io->Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    char error[1024];
    program = agl_create(vertex_shader, fragment_shader, error, sizeof(error));
    assert(program);

    attribs[attrib_pos] = agl_get_attrib_location(program, "pos");
    attribs[attrib_uv] = agl_get_attrib_location(program, "uv");
    attribs[attrib_color] = agl_get_attrib_location(program, "color");
    check_bindings(attribs, attrib_count);

    uniforms[uniform_tex] = agl_get_uniform_location(program, "tex");
    uniforms[uniform_proj_mat] = agl_get_uniform_location(program, "proj_mat");
    check_bindings(uniforms, uniform_count);

    glGenBuffersARB(1, &vbo);
    glGenBuffersARB(1, &elements);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBufferARB(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArrayARB(attribs[attrib_pos]);
    glEnableVertexAttribArrayARB(attribs[attrib_uv]);
    glEnableVertexAttribArrayARB(attribs[attrib_color]);

    glVertexAttribPointerARB(attribs[attrib_pos], 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid *)offset_of(ImDrawVert, pos));
    glVertexAttribPointerARB(attribs[attrib_uv], 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid *)offset_of(ImDrawVert, uv));
    glVertexAttribPointerARB(attribs[attrib_color], 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid *)offset_of(ImDrawVert, col));

    glGenTextures(1, &font_texture);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    io->Fonts->TexID = (void *)(intptr)font_texture;
}

static void imgui_update(Input *input, i32 window_width, i32 window_height)
{
    ImGuiIO *io = get_imgui_io();

    io->DisplaySize = ImVec2((f32)window_width, (f32)window_height);
    io->DeltaTime = input->dt;

    io->MousePos = ImVec2(input->mouse_x, input->mouse_y);
    io->MouseDown[0] = input->mouse_buttons[mouse_button_left] != 0;
    io->MouseDown[1] = input->mouse_buttons[mouse_button_right] != 0;
    io->MouseDown[2] = input->mouse_buttons[mouse_button_middle] != 0;

    io->MouseWheel = input->mouse_z;

    io->KeyCtrl = input->control_down != 0;
    io->KeyShift = input->shift_down != 0;
    io->KeyAlt = input->alt_down != 0;

    for (u32 button_index = 0; button_index < button_count; ++button_index)
    {
        io->KeysDown[button_index] = input->buttons[button_index] != 0;
    }

    ImGui::NewFrame();
}

static void imgui_render()
{
    ImGuiIO *io = get_imgui_io();
    ImVec4 clear_color = ImColor(114, 144, 154);

    i32 fb_width = (i32)(io->DisplaySize.x);
    i32 fb_height = (i32)(io->DisplaySize.y);

    glViewport(0, 0, fb_width, fb_height);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui::Render();
    ImDrawData *data = ImGui::GetDrawData();

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glActiveTextureARB(GL_TEXTURE0);

    // NOTE(dan): ortho proj matrix
    glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
    
    f32 proj_mat[4][4] =
    {
        { 2.0f / io->DisplaySize.x, 0.0f,                      0.0f, 0.0f },
        { 0.0f,                     2.0f / -io->DisplaySize.y, 0.0f, 0.0f },
        { 0.0f,                     0.0f,                     -1.0f, 0.0f },
        {-1.0f,                     1.0f,                      0.0f, 1.0f },
    };

    agl_use_program(program);
    glUniform1iARB(uniforms[uniform_tex], 0);
    glUniformMatrix4fvARB(uniforms[uniform_proj_mat], 1, GL_FALSE, &proj_mat[0][0]);
    glBindVertexArray(vao);

    for (i32 cmd_index = 0; cmd_index < data->CmdListsCount; ++cmd_index)
    {
        ImDrawList *list = data->CmdLists[cmd_index];
        ImDrawIdx *index_buff_offset = 0;

        glBindBufferARB(GL_ARRAY_BUFFER, vbo);
        glBufferDataARB(GL_ARRAY_BUFFER, (GLsizeiptr)list->VtxBuffer.size() * sizeof(ImDrawVert), (GLvoid *)&list->VtxBuffer.front(), GL_STREAM_DRAW);

        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER, elements);
        glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)list->IdxBuffer.size() * sizeof(ImDrawIdx), (GLvoid *)&list->IdxBuffer.front(), GL_STREAM_DRAW);

        for (ImDrawCmd *cmd = list->CmdBuffer.begin(); cmd != list->CmdBuffer.end(); ++cmd)
        {
            if (cmd->UserCallback)
            {
                cmd->UserCallback(list, cmd);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr)cmd->TextureId);
                glDrawElements(GL_TRIANGLES, (GLsizei)cmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, index_buff_offset);
            }

            index_buff_offset += cmd->ElemCount;
        }
    }
}

struct PermanentState
{
    AudioState audio_state;

    b32 initialized;
};

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

static UPDATE_AND_RENDER_PROC(update_and_render)
{
    PermanentState *permanent_state = get_or_create_permanent_state(memchunk);
    if (!permanent_state->initialized)
    {
        init_imgui();

        permanent_state->initialized = true;
    }
    
    imgui_update(input, window_width, window_height);

    // NOTE(dan): update

    {
        ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiSetCond_FirstUseEver);

        static bool open = true;
        ImGui::Begin("audio", &open, 0);

        ImGui::End();
    }

    // NOTE(dan): render

    imgui_render();
}

static AudioRecord *start_recording(AudioState *state, u16 num_channels, u16 bits_per_sample, u32 samples_per_sec)
{
    AudioRecord *record = &state->record;

    record->num_channels = num_channels;
    record->bits_per_sample = bits_per_sample;
    record->samples_per_sec = samples_per_sec;
    record->write_cursor = 0;
    record->read_cursor = 0;
    record->buffer_size = record->samples_per_sec * record->num_channels * (record->bits_per_sample / 8);
    record->buffer = push_size(&state->record_memory, record->buffer_size);

    record->streaming = true;
    return record;
}

static RECORD_AUDIO_PROC(record_audio)
{
    PermanentState *permanent_state = get_or_create_permanent_state(memchunk);
    AudioState *audio_state = &permanent_state->audio_state;

    if (!audio_state->recording_initialized)
    {
        sub_memchunk(&audio_state->record_memory, memchunk, 1*MB);
        start_recording(audio_state, 2, 16, 48000);

        audio_state->recording_initialized = true;
    }

    // TODO(dan): simplify this
    usize remaining = audio_state->record.buffer_size - audio_state->record.write_cursor;

    if (size1 > remaining)
    {
        memcpy((u8 *)audio_state->record.buffer + audio_state->record.write_cursor, buffer1, remaining);
        memcpy(audio_state->record.buffer, buffer1, size1 - remaining);

        audio_state->record.write_cursor = size1;
    }
    else
    {
        memcpy((u8 *)audio_state->record.buffer + audio_state->record.write_cursor, buffer1, size1);

        audio_state->record.write_cursor += size1;
    }

    /*
    if (size2)
    {
        remaining = audio_state->record.buffer_size - audio_state->record.write_cursor;

        if (size2 > remaining)
        {
            memcpy(buffer2, (u8 *)audio_state->record.buffer + audio_state->record.write_cursor, remaining);
            memcpy(buffer2, audio_state->record.buffer, size2 - remaining);

            audio_state->record.write_cursor = size2;
        }
        else
        {
            memcpy(buffer2, (u8 *)audio_state->record.buffer + audio_state->record.write_cursor, size2);

            audio_state->record.write_cursor += size2;
        }
    }
    */
}

static AudioStream *play_audio(AudioState *state, u16 num_channels, u16 bits_per_sample, u32 samples_per_sec, void *buffer, u32 num_samples)
{
    if (!state->first_free)
    {
        state->first_free = push_struct(&state->mixer_memory, AudioStream);
        state->first_free->next = 0;
    }

    AudioStream *audio_stream = state->first_free;
    state->first_free = audio_stream->next;
    
    audio_stream->num_channels = num_channels;
    audio_stream->bits_per_sample = bits_per_sample;
    audio_stream->samples_per_sec = samples_per_sec;
    audio_stream->buffer = buffer;
    audio_stream->samples_played = 0;
    audio_stream->num_samples = num_samples;

    audio_stream->next = state->first;
    state->first = audio_stream;

    return audio_stream;
}

static MIX_AUDIO_PROC(mix_audio)
{
    PermanentState *permanent_state = get_or_create_permanent_state(memchunk);
    AudioState *state = &permanent_state->audio_state;

    if (!state->playback_initialized)
    {
        sub_memchunk(&state->mixer_memory, memchunk, 1*MB);

        #if 0
        LoadedFile file = win32_load_file("w:\\ani\\data\\test3.wav");
        ParsedWav wav = parse_wav(file.contents, file.size);

        state->music = play_audio(state, wav.num_channels, wav.bits_per_sample, wav.samples_per_sec, 
                                        wav.samples, wav.num_samples);
        #endif
        state->playback_initialized = true;
    }

    // TODO(dan): SIMD this
    TempMemchunk mixer_memory = begin_temp_memchunk(&state->mixer_memory);
    
    f32 *channel0 = push_array(&state->mixer_memory, num_samples, f32, align_no_clear(4));
    f32 *channel1 = push_array(&state->mixer_memory, num_samples, f32, align_no_clear(4));

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

    if (state->record.streaming)
    {
        u32 play_size = (state->record.write_cursor - state->record.read_cursor) % state->record.buffer_size;

        if (play_size > num_samples)
        {
            play_size = num_samples;
        }

        f32 *dest0 = channel0;
        f32 *dest1 = channel1;

        for (u32 sample_index = 0; sample_index < play_size; ++sample_index)
        {
            u32 source_index = (state->record.read_cursor + sample_index) % state->record.buffer_size;
            i16 *source = (i16 *)((u32 *)state->record.buffer + source_index);

            if (*source)
            {
                int blockhere = 1;
            }

            *dest0++ = *source++;
            *dest1++ = *source++;
        }

        state->record.read_cursor += play_size;
        state->record.read_cursor %= state->record.buffer_size;
    }

    #if 0
    // NOTE(dan): mix 
    for (AudioStream **stream_ptr = &state->first; *stream_ptr; )
    {
        AudioStream *stream = *stream_ptr;
        b32 finished = false;

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
