#ifndef WIN32_ANI_GL_H

#define AGL_ARB
#define AGL_EXTLIST         "ani_gl_extlist.txt"
#define WIN32_AGL_EXTLIST   "win32_ani_gl_extlist.txt"

#define AGL_GET_FUNC(func) wglGetProcAddress(func)

#include "ani_gl.h"

#define WGL_DRAW_TO_WINDOW_ARB      0x2001
#define WGL_ACCELERATION_ARB        0x2003
#define WGL_SUPPORT_OPENGL_ARB      0x2010
#define WGL_DOUBLE_BUFFER_ARB       0x2011
#define WGL_PIXEL_TYPE_ARB          0x2013
#define WGL_COLOR_BITS_ARB          0x2014
#define WGL_DEPTH_BITS_ARB          0x2022
#define WGL_STENCIL_BITS_ARB        0x2023
#define WGL_TYPE_RGBA_ARB           0x202B
#define WGL_FULL_ACCELERATION_ARB   0x2027

#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB  0x20A9

#define WGL_CONTEXT_MAJOR_VERSION_ARB   0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB   0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB     0x2093
#define WGL_CONTEXT_FLAGS_ARB           0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB    0x9126

#define WGL_CONTEXT_DEBUG_BIT_ARB                   0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB      0x0002
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB            0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB   0x00000002

#define WGL_DECLARE(type, name, ...) typedef type (__stdcall *PFNWGL##name##PROC)(__VA_ARGS__)
#define WGLARB_DECLARE(type, name, ...) WGL_DECLARE(type, name##ARB, __VA_ARGS__)
#define WGLEXT_DECLARE(type, name, ...) WGL_DECLARE(type, name##EXT, __VA_ARGS__)

WGLARB_DECLARE(HGLRC,  CREATECONTEXTATTRIBS, HDC, HGLRC, int *);
WGLARB_DECLARE(int,    CHOOSEPIXELFORMAT,    HDC, int *, f32 *, uint, int *, uint *);
WGLEXT_DECLARE(int,    SWAPINTERVAL,         int);

#define WGLARB(a, b) WGLE(a##ARB, b##ARB)
#define WGLEXT(a, b) WGLE(a##EXT, b##EXT)

#define WGLE(a, b) static PFNWGL##a##PROC wgl##b
#include WIN32_AGL_EXTLIST
#undef WGLE

static void win32_agl_init_extensions()
{
    WNDCLASSEXA window_class = {0};
    window_class.cbSize        = sizeof(window_class);
    window_class.style         = CS_OWNDC;
    window_class.lpfnWndProc   = DefWindowProcA;
    window_class.hInstance     = GetModuleHandleA(0);
    window_class.lpszClassName = "AniDummyWindowForExtensionLoader";
    
    if (RegisterClassExA(&window_class))
    {
        HWND window = CreateWindowExA(0, window_class.lpszClassName, "AniExtensionLoader", 0,
                                      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                      0, 0, window_class.hInstance, 0);
        if (window)
        {
            PIXELFORMATDESCRIPTOR descriptor = {0};
            descriptor.nSize        = sizeof(descriptor);
            descriptor.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
            descriptor.iPixelType   = PFD_TYPE_RGBA;
            descriptor.cColorBits   = 32;
            descriptor.cDepthBits   = 24;
            descriptor.cStencilBits = 8;
            descriptor.iLayerType   = PFD_MAIN_PLANE;

            HDC dc = GetDC(window);
            i32 pixel_format_index = ChoosePixelFormat(dc, &descriptor);            
            SetPixelFormat(dc, pixel_format_index, &descriptor);

            HGLRC rc = wglCreateContext(dc);
            if (wglMakeCurrent(dc, rc))
            {
                // NOTE(dan): load extensions
                #define WGLE(a, b) wgl##b = (PFNWGL##a##PROC)AGL_GET_FUNC("wgl" #b);
                #include WIN32_AGL_EXTLIST
                #undef WGLE

                agl_prog_init();
                agl_init_extensions();

                wglMakeCurrent(0, 0);
            }

            wglDeleteContext(rc);
            ReleaseDC(window, dc);
            DestroyWindow(window);
        }
    }
}

static HGLRC win32_agl_create_context(HDC dc, i32 *pixel_format_attribs, i32 *context_attribs)
{
    HGLRC rc = 0;

    if (wglChoosePixelFormatARB)
    {
        PIXELFORMATDESCRIPTOR pixel_format = {0};
        i32 pixel_format_index = 0;
        GLuint extended = 0;

        wglChoosePixelFormatARB(dc, pixel_format_attribs, 0, 1, &pixel_format_index, &extended);
        DescribePixelFormat(dc, pixel_format_index, sizeof(pixel_format), &pixel_format);
        SetPixelFormat(dc, pixel_format_index, &pixel_format);

        if (wglCreateContextAttribsARB)
        {
            rc = wglCreateContextAttribsARB(dc, 0, context_attribs);
        }
    }
    else
    {
        assert(!"Call win32_agl_init_extensions() before win32_agl_create_context()");
    }

    return rc;
}

inline void win32_agl_set_interval(i32 interval)
{
    if (wglSwapIntervalEXT)
    {
        wglSwapIntervalEXT(interval);
    }
}

#define WIN32_ANI_GL_H
#endif
