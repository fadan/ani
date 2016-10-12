#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H

#pragma warning(push)
#pragma warning(disable: 4459)
#include "imgui.h"

#include "imgui_demo.cpp"
#include "imgui_draw.cpp"
#include "imgui.cpp"
#pragma warning(pop)

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

GLuint ui_program;

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

static void init_ui()
{
    ImGuiIO *io = get_imgui_io();
    u8 *pixels;
    i32 width, height;    
    
    io->Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    char error[1024];
    ui_program = agl_create(vertex_shader, fragment_shader, error, sizeof(error));
    assert(ui_program);

    attribs[attrib_pos] = agl_get_attrib_location(ui_program, "pos");
    attribs[attrib_uv] = agl_get_attrib_location(ui_program, "uv");
    attribs[attrib_color] = agl_get_attrib_location(ui_program, "color");
    check_bindings(attribs, attrib_count);

    uniforms[uniform_tex] = agl_get_uniform_location(ui_program, "tex");
    uniforms[uniform_proj_mat] = agl_get_uniform_location(ui_program, "proj_mat");
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

static void begin_ui(Input *input, i32 window_width, i32 window_height)
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

static void end_ui()
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

    agl_use_program(ui_program);
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
