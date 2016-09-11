#include "ani_platform.h"

#if INTERNAL_BUILD
    #define MEMORY_BASE_ADDRESS    (void *)(2*TB)
#else
    #define MEMORY_BASE_ADDRESS    (void *)0
#endif

#define PLATFORM_MEMORY_SIZE    8*MB
#define PERMANENT_MEMORY_SIZE   8*MB

#define UPDATE_HZ       60
#define TIMESTEP_SEC    (1.0f / UPDATE_HZ)

#define INITGUID
#include "win32_ani_platform.h"

static b32 global_quit;

static void win32_init_window(Win32Window *window)
{
    WNDCLASSEXA window_class = {0};
    window_class.cbSize        = sizeof(window_class);
    window_class.style         = CS_OWNDC;
    window_class.lpfnWndProc   = window->wndproc;
    window_class.hInstance     = GetModuleHandleA(0);
    window_class.hCursor       = LoadCursorA(0, IDC_ARROW);
    window_class.lpszClassName = "AniWindowClass";

    if (RegisterClassExA(&window_class))
    {
        window->wnd = CreateWindowExA(0, window_class.lpszClassName, window->title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT, CW_USEDEFAULT, window->width, window->height, 
                                      0, 0, window_class.hInstance, 0);
        if (window->wnd)
        {
            i32 pixel_format_attribs[] =
            {
                WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
                WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
                WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
                WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
                WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
                WGL_COLOR_BITS_ARB,     24,
                WGL_DEPTH_BITS_ARB,     24,
                WGL_STENCIL_BITS_ARB,   8,
                0
            };

            #if INTERNAL_BUILD
                #define CONTEXT_FLAGS WGL_CONTEXT_DEBUG_BIT_ARB 
            #else
                #define CONTEXT_FLAGS WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
            #endif

            i32 context_attribs[] =
            {
                WGL_CONTEXT_MAJOR_VERSION_ARB,  3,
                WGL_CONTEXT_MINOR_VERSION_ARB,  1,
                WGL_CONTEXT_FLAGS_ARB,          CONTEXT_FLAGS,
                WGL_CONTEXT_PROFILE_MASK_ARB,   WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
                0,
            };

            win32_agl_init_extensions();

            window->dc = GetDC(window->wnd);
            window->rc = win32_agl_create_context(window->dc, pixel_format_attribs, context_attribs);
            window->context_initialized = wglMakeCurrent(window->dc, window->rc);
        }
    }
}

static void win32_init_state(Win32State *state)
{
    usize total_memory_size = PLATFORM_MEMORY_SIZE + PERMANENT_MEMORY_SIZE;
    void *memory = win32_allocate(total_memory_size, MEMORY_BASE_ADDRESS);

    Memchunk total_memory;
    init_memchunk(&total_memory, memory, total_memory_size);

    sub_memchunk(&state->platform_memory, &total_memory, PLATFORM_MEMORY_SIZE);
    sub_memchunk(&state->permanent_memory, &total_memory, PERMANENT_MEMORY_SIZE);

    state->initialized = (memory != 0);
}

inline u32 win32_translate_key(WPARAM wparam, LPARAM lparam)
{
    u32 button = invalid_button;
    u32 vk = (u32)wparam;
    switch (vk)
    {
        case VK_TAB:    { button = button_tab; } break;
        case VK_LEFT:   { button = button_left; } break;
        case VK_RIGHT:  { button = button_right; } break;
        case VK_UP:     { button = button_up; } break;
        case VK_DOWN:   { button = button_down; } break;
        case VK_PRIOR:  { button = button_pageup; } break;
        case VK_NEXT:   { button = button_pagedown; } break;
        case VK_HOME:   { button = button_home; } break;
        case VK_END:    { button = button_end; } break;
        case VK_DELETE: { button = button_delete; } break;
        case VK_BACK:   { button = button_backspace; } break;
        case VK_RETURN: { button = button_enter; } break;
        case VK_ESCAPE: { button = button_esc; } break;
        case 'A':       { button = button_a; } break;
        case 'C':       { button = button_c; } break;
        case 'V':       { button = button_v; } break;
        case 'X':       { button = button_x; } break;
        case 'Y':       { button = button_y; } break;
        case 'Z':       { button = button_z; } break;
    }
    return button;
}

static void win32_process_messages(Input *input)
{
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
    {
        WPARAM wparam = msg.wParam;
        LPARAM lparam = msg.lParam;
        UINT message = msg.message;

        switch (message)
        {
            case WM_QUIT:
            {
                global_quit = true;
            } break;

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP:
            {
                u32 key = win32_translate_key(wparam, lparam);
                b32 was_down = ((lparam & (1 << 30)) != 0);
                b32 is_down = ((lparam & (1 << 31)) == 0);

                if ((was_down != is_down) && (key != invalid_button))
                {
                    input->buttons[key] = is_down;
                }
            } break;

            case WM_LBUTTONDOWN: { input->mouse_buttons[mouse_button_left] = true; } break;
            case WM_RBUTTONDOWN: { input->mouse_buttons[mouse_button_right] = true; } break;
            case WM_MBUTTONDOWN: { input->mouse_buttons[mouse_button_middle] = true; } break;
            case WM_XBUTTONDOWN:
            {
                u32 button = ((GET_XBUTTON_WPARAM(wparam) == XBUTTON1) ? mouse_button_extended1 : mouse_button_extended0);
                input->mouse_buttons[button] = true;
            } break;

            case WM_LBUTTONUP: { input->mouse_buttons[mouse_button_left] = false; } break;
            case WM_RBUTTONUP: { input->mouse_buttons[mouse_button_right] = false; } break;
            case WM_MBUTTONUP: { input->mouse_buttons[mouse_button_middle] = false; } break;
            case WM_XBUTTONUP:
            {
                u32 button = ((GET_XBUTTON_WPARAM(wparam) == XBUTTON1) ? mouse_button_extended1 : mouse_button_extended0);
                input->mouse_buttons[button] = false;
            } break;

            case WM_MOUSEWHEEL:
            {
                input->mouse_z = (f32)GET_WHEEL_DELTA_WPARAM(wparam) / (f32)WHEEL_DELTA;
            } break;

            default:
            {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            } break;
        }
    }
}

static LRESULT CALLBACK win32_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    switch (message)
    {
        case WM_CLOSE:
        case WM_DESTROY:
        {
            global_quit = true;
        } break;
        default:
        {
            result = DefWindowProcA(window, message, wparam, lparam);
        } break;
    }
    return result;
}

static void win32_update_input(Win32Window *window, Input *current_input, Input *previous_input)
{
    *current_input = {};

    for (u32 button_index = 0; button_index < button_count; ++button_index)
    {
        current_input->buttons[button_index] = previous_input->buttons[button_index];
    }

    for (u32 button_index = 0; button_index < mouse_button_count; ++button_index)
    {
        current_input->mouse_buttons[button_index] = previous_input->mouse_buttons[button_index];
    }
    
    win32_process_messages(current_input);

    current_input->shift_down = (GetKeyState(VK_SHIFT) & (1 << 15));
    current_input->alt_down = (GetKeyState(VK_MENU) & (1 << 15));
    current_input->control_down = (GetKeyState(VK_CONTROL) & (1 << 15));

    current_input->dt = TIMESTEP_SEC;

    POINT mouse_pos;
    GetCursorPos(&mouse_pos);
    ScreenToClient(window->wnd, &mouse_pos);

    current_input->mouse_x = (f32)mouse_pos.x;
    current_input->mouse_y = (f32)mouse_pos.y + 40; // TODO(dan): fix this
}

int WinMain(HINSTANCE instance, HINSTANCE prev_instance, char *cmdline, int cmd_show)
{
    win32_init_net();

    Win32State state = {0};
    state.mix_audio         = mix_audio;
    state.record_audio      = record_audio;
    state.update_and_render = update_and_render;
    win32_init_state(&state);

    Win32Window window = {0};
    window.title   = "Ani";
    window.width   = 1280;
    window.height  = 720;
    window.wndproc = (WNDPROC)win32_window_proc;
    win32_init_window(&window);

    Win32Audio audio = {0};
    audio.num_channels    = 2;
    audio.bits_per_sample = 16;
    audio.samples_per_sec = 48000;
    win32_init_audio(&audio, window.wnd);

    Input inputs[2] = {0};
    Input *current_input = &inputs[0];
    Input *previous_input = &inputs[1];

    i16 server_port = 30000;
    i16 client_port = 30001;
    i32 protocol_id = 0x99887766;
    f32 timeout_secs = 10.0f;

    Win32Connection server = {0};
    server.socket       = win32_create_socket(server_port);
    server.state        = ConnectionState_Listening;
    server.protocol_id  = protocol_id;
    server.timeout_secs = timeout_secs;
    server.is_server    = true;

    Win32Connection client = {0};
    client.socket       = win32_create_socket(client_port);
    client.state        = ConnectionState_Connecting;
    client.protocol_id  = protocol_id;
    client.timeout_secs = timeout_secs;
    client.ip           = IPV4_TO_U32(127, 0, 0, 1);
    client.port         = server_port;

    if (window.context_initialized && state.initialized)
    {
        f32 carried_dt = 0;
        f32 time = 0;

        win32_agl_set_interval(1);

        while (!global_quit)
        {
            f32 current_time = win32_get_time();
            f32 dt = current_time - time;

            time = current_time;
            carried_dt += dt;

            while (carried_dt > TIMESTEP_SEC)
            {
                carried_dt -= TIMESTEP_SEC;

                // NOTE(dan): test connection
                {
                    if (client.state == ConnectionState_Connected && server.state == ConnectionState_Connected)
                    {
                        int breakhere = 1;
                    }

                    if (client.state == ConnectionState_ConnectionFailed)
                    {
                        int breakhere = 2;
                    }

                    char client_packet[] = "client to server";
                    win32_send_packet(&state.platform_memory, &client, client_packet, array_size(client_packet));

                    char server_packet[] = "server to client";
                    win32_send_packet(&state.platform_memory, &server, server_packet, array_size(server_packet));

                    char packet[256];
                    i32 packet_size;
                    while (0 < (packet_size = win32_receive_packet(&state.platform_memory, &client, packet, sizeof(packet))))
                    {
                        int breakhere = 3;
                    }

                    while (0 < (packet_size = win32_receive_packet(&state.platform_memory, &server, packet, sizeof(packet))))
                    {
                        int breakhere = 4;
                    }

                    win32_update_connection(&client, TIMESTEP_SEC);
                    win32_update_connection(&server, TIMESTEP_SEC);
                }

                win32_update_input(&window, current_input, previous_input);              
                win32_update_audio(&state, &audio);

                RECT rect;
                GetWindowRect(window.wnd, &rect);
                window.width = rect.right - rect.left;
                window.height = rect.bottom - rect.top;

                state.update_and_render(&state.permanent_memory, current_input, window.width, window.height);

                Input *temp_input = current_input;
                current_input = previous_input;
                previous_input = temp_input;
                
                SwapBuffers(window.dc);
            }
        }
    }

    win32_shutdown_net();
}
