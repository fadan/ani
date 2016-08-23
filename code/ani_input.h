#ifndef ANI_INPUT_H

enum
{
    button_tab,
    button_left,
    button_right,
    button_up,
    button_down,
    button_pageup,
    button_pagedown,
    button_home,
    button_end,
    button_delete,
    button_backspace,
    button_enter,
    button_esc,
    button_a,
    button_c,
    button_v,
    button_x,
    button_y,
    button_z,

    button_count,
};

enum
{
    mouse_button_left,
    mouse_button_middle,
    mouse_button_right,
    mouse_button_extended0,
    mouse_button_extended1,

    mouse_button_count,
};

#define invalid_button          button_count
#define invalid_mouse_button    mouse_button_count

struct Input
{
    f32 dt;
    f32 mouse_x;
    f32 mouse_y;
    f32 mouse_z;
    
    b32 shift_down;
    b32 alt_down;
    b32 control_down;

    b32 buttons[button_count];
    b32 mouse_buttons[mouse_button_count];
};

#define ANI_INPUT_H
#endif
