#ifndef ANI_GL_H

#ifndef AGL_EXTLIST
    #define AGL_EXTLIST    "ani_gl_extlist.txt"
#endif

#include <gl/gl.h>
#include "glext.h"

#ifdef AGL_ARB
    #define AGL_VERTEX_SHADER       GL_VERTEX_SHADER_ARB
    #define AGL_FRAGMENT_SHADER     GL_FRAGMENT_SHADER_ARB
#else
    #define AGL_VERTEX_SHADER       GL_VERTEX_SHADER
    #define AGL_FRAGMENT_SHADER     GL_FRAGMENT_SHADER
#endif

#define AGL_PROG_EXTENSIONS \
    AGL_PROG_FUNC(ATTACHOBJECT,           AttachObject) \
    AGL_PROG_FUNC(BINDATTRIBLOCATION,     BindAttribLocation) \
    AGL_PROG_FUNC(COMPILESHADER,          CompileShader) \
    AGL_PROG_FUNC(CREATEPROGRAMOBJECT,    CreateProgramObject) \
    AGL_PROG_FUNC(CREATESHADEROBJECT,     CreateShaderObject) \
    AGL_PROG_FUNC(DELETEOBJECT,           DeleteObject) \
    AGL_PROG_FUNC(DETACHOBJECT,           DetachObject) \
    AGL_PROG_FUNC(GETINFOLOG,             GetInfoLog) \
    AGL_PROG_FUNC(GETOBJECTPARAMETERIV,   GetObjectParameteriv) \
    AGL_PROG_FUNC(LINKPROGRAM,            LinkProgram) \
    AGL_PROG_FUNC(USEPROGRAMOBJECT,       UseProgramObject) \
    AGL_PROG_FUNC(SHADERSOURCE,           ShaderSource) \
    \
    AGL_PROG_FUNC(GETATTRIBLOCATION,      GetAttribLocation) \
    AGL_PROG_FUNC(GETUNIFORMLOCATION,     GetUniformLocation) \
    AGL_PROG_FUNC(UNIFORM1I,              Uniform1i) \
    AGL_PROG_FUNC(UNIFORM1F,              Uniform1f) \
    AGL_PROG_FUNC(UNIFORMMATRIX4FV,       UniformMatrix4fv) \
    AGL_PROG_FUNC(UNIFORM4FV,             Uniform4fv)

#define AGL_PROG_FUNC(a, b)   static PFNGL##a##ARBPROC gl##b##ARB;
AGL_PROG_EXTENSIONS
#undef AGL_PROG_FUNC

#ifndef AGL_GET_FUNC
    // NOTE(dan): for example on win32 platform: #define AGL_GET_FUNC(func) wglGetProcAddress(func)
    #error "Define AGL_GET_FUNC() before including ani_gl.h"
#endif

inline void agl_prog_init()
{
    #define AGL_PROG_FUNC(a, b)   gl##b##ARB = (PFNGL##a##ARBPROC)AGL_GET_FUNC("gl" #b "ARB");
    AGL_PROG_EXTENSIONS
    #undef AGL_PROG_FUNC
}

#undef AGL_PROG_EXTENSIONS

#ifdef AGL_ARB
    #define agl_attach_shader               glAttachObjectARB
    #define agl_compile_shader              glCompileShaderARB
    #define agl_create_program              glCreateProgramObjectARB
    #define agl_create_shader               glCreateShaderObjectARB
    #define agl_delete_shader               glDeleteObjectARB
    #define agl_get_attrib_location         glGetAttribLocationARB
    #define agl_get_program_status(a, b)    glGetObjectParameterivARB(a, GL_OBJECT_LINK_STATUS_ARB, b)
    #define agl_get_shader_info_log         glGetInfoLogARB
    #define agl_get_shader_status(a, b)     glGetObjectParameterivARB(a, GL_OBJECT_COMPILE_STATUS_ARB, b)
    #define agl_get_uniform_location        glGetUniformLocationARB
    #define agl_link_program                glLinkProgramARB
    #define agl_shader_source               glShaderSourceARB
    #define agl_use_program                 glUseProgramObjectARB
#else
    #define agl_attach_shader               glAttachShader
    #define agl_compile_shader              glCompileShader
    #define agl_create_program              glCreateProgram
    #define agl_create_shader               glCreateShader
    #define agl_delete_shader               glDeleteShader
    #define agl_get_attrib_location         glGetAttribLocation
    #define agl_get_program_status(a, b)    glGetShaderiv(a, GL_LINK_STATUS, b)
    #define agl_get_shader_info_log         glGetShaderInfoLog
    #define agl_get_shader_status(a, b)     glGetShaderiv(a, GL_COMPILE_STATUS, b)
    #define agl_get_uniform_location        glGetUniformLocation
    #define agl_link_program                glLinkProgram
    #define agl_shader_source               glShaderSource
    #define agl_use_program                 glUseProgram
#endif

static GLuint agl_compile(char const *source, GLenum type, char *error, u32 error_length)
{
    GLuint handle = agl_create_shader(type);
    agl_shader_source(handle, 1, &source, 0);
    agl_compile_shader(handle);

    GLint status;
    agl_get_shader_status(handle, &status);

    if (!status)
    {
        agl_get_shader_info_log(handle, error_length, 0, error);
        invalid_codepath;
    }

    return handle;
}

static GLuint agl_create(char *vertex_source, char *fragment_source, char *error, u32 error_length)
{
    GLuint handle = agl_create_program();

    if (vertex_source)
    {
        GLuint vertex_shader = agl_compile(vertex_source, AGL_VERTEX_SHADER, error, error_length);
        agl_attach_shader(handle, vertex_shader);
        agl_delete_shader(vertex_shader);
    }
    if (fragment_source)
    {
        GLuint fragment_shader = agl_compile(fragment_source, AGL_FRAGMENT_SHADER, error, error_length);
        agl_attach_shader(handle, fragment_shader);
        agl_delete_shader(fragment_shader);
    }

    agl_link_program(handle);

    GLint status;
    agl_get_program_status(handle, &status);

    if (!status)
    {
        agl_get_shader_info_log(handle, error_length, 0, error);
        invalid_codepath;
    }

    return handle;
}

#define GLARB(a, b) GLE(a##ARB, b##ARB)
#define GLEXT(a, b) GLE(a##EXT, b##EXT)

#define GLE(a, b) PFNGL##a##PROC gl##b;
#include AGL_EXTLIST
#undef GLE

inline void agl_init_extensions()
{
    #define GLE(a, b) gl##b = (PFNGL##a##PROC)AGL_GET_FUNC("gl" #b);
    #include AGL_EXTLIST
    #undef GLE
}

#define ANI_GL_H
#endif
