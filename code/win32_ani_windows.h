#ifndef WIN32_ANI_WINDOWS_H

#define DECLARE_HANDLE(name) struct name##__{int unused;}; typedef struct name##__ *name
#define MAKEINTRESOURCEA(i) ((char *)((ulong)((ushort)(i))))

#define CS_VREDRAW                  0x0001
#define CS_HREDRAW                  0x0002
#define CS_OWNDC                    0x0020

#define CW_USEDEFAULT               ((int)0x80000000)

#define GWL_STYLE                   (-16)

#define HWND_TOP                    ((HWND)0)

#define IDC_ARROW                   MAKEINTRESOURCEA(32512)

#define MEM_COMMIT                  0x1000
#define MEM_RESERVE                 0x2000
#define MEM_RELEASE                 0x8000

#define MONITOR_DEFAULTTOPRIMARY    0x00000001

#define PAGE_READWRITE              0x04

#define PFD_DOUBLEBUFFER            0x00000001
#define PFD_DRAW_TO_WINDOW          0x00000004
#define PFD_SUPPORT_OPENGL          0x00000020
#define PFD_TYPE_RGBA               0
#define PFD_MAIN_PLANE              0

#define PM_REMOVE                   0x0001

#define SWP_NOSIZE                  0x0001
#define SWP_NOMOVE                  0x0002
#define SWP_NOZORDER                0x0004
#define SWP_FRAMECHANGED            0x0020
#define SWP_NOOWNERZORDER           0x0200

#define VK_RETURN                   0x0D
#define VK_ESCAPE                   0x1B

#define WM_DESTROY                  0x0002
#define WM_CLOSE                    0x0010
#define WM_QUIT                     0x0012
#define WM_SYSKEYDOWN               0x0104
#define WM_SYSKEYUP                 0x0105
#define WM_KEYDOWN                  0x0100
#define WM_KEYUP                    0x0101

#define WS_OVERLAPPED               0x00000000L
#define WS_CAPTION                  0x00C00000L
#define WS_SYSMENU                  0x00080000L
#define WS_THICKFRAME               0x00040000L
#define WS_MINIMIZEBOX              0x00020000L
#define WS_MAXIMIZEBOX              0x00010000L
#define WS_VISIBLE                  0x10000000L
#define WS_OVERLAPPEDWINDOW         (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)

DECLARE_HANDLE(HBRUSH);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HGLRC);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HWND);

typedef int (__stdcall *voidfunc)(void);
typedef longptr (__stdcall *WNDPROC)(HWND, uint, uintptr, longptr);

typedef HICON HCURSOR;
typedef HINSTANCE HMODULE;
typedef void *HMONITOR;

typedef struct
{
    uint      cbSize;
    uint      style;
    WNDPROC   lpfnWndProc;
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    char *    lpszMenuName;
    char *    lpszClassName;
    HICON     hIconSm;
} WNDCLASSEXA;

typedef struct
{
    long x;
    long y;
} POINT;

typedef struct
{
    HWND    hwnd;
    uint    message;
    uintptr wParam;
    longptr lParam;
    ulong   time;
    POINT   pt;
} MSG;

typedef struct
{
    long  left;
    long  top;
    long  right;
    long  bottom;
} RECT;

typedef struct
{
    ulong  cbSize;
    RECT   rcMonitor;
    RECT   rcWork;
    uint   dwFlags;
} MONITORINFO;

typedef struct
{
    uint  length;
    uint  flags;
    uint  showCmd;
    POINT ptMinPosition;
    POINT ptMaxPosition;
    RECT  rcNormalPosition;
} WINDOWPLACEMENT;

typedef union
{
    struct 
    {
        ulong LowPart;
        long  HighPart;
    };
    struct 
    {
        ulong LowPart;
        long  HighPart;
    } u;
    i64 QuadPart;
} LARGE_INTEGER;

typedef struct
{
    ushort  nSize;
    ushort  nVersion;
    ulong   dwFlags;
    uchar   iPixelType;
    uchar   cColorBits;
    uchar   cRedBits;
    uchar   cRedShift;
    uchar   cGreenBits;
    uchar   cGreenShift;
    uchar   cBlueBits;
    uchar   cBlueShift;
    uchar   cAlphaBits;
    uchar   cAlphaShift;
    uchar   cAccumBits;
    uchar   cAccumRedBits;
    uchar   cAccumGreenBits;
    uchar   cAccumBlueBits;
    uchar   cAccumAlphaBits;
    uchar   cDepthBits;
    uchar   cStencilBits;
    uchar   cAuxBuffers;
    uchar   iLayerType;
    uchar   bReserved;
    ulong   dwLayerMask;
    ulong   dwVisibleMask;
    ulong   dwDamageMask;
} PIXELFORMATDESCRIPTOR;

ani_import int      __stdcall ChoosePixelFormat(HDC, PIXELFORMATDESCRIPTOR *);
ani_import HWND     __stdcall CreateWindowExA(ulong, char *, char *, ulong, int, int, int, int, HWND, HMENU, HINSTANCE, void *);
ani_import longptr  __stdcall DefWindowProcA(HWND, uint, uintptr, longptr);
ani_import int      __stdcall DescribePixelFormat(HDC, int, uint, PIXELFORMATDESCRIPTOR *);
ani_import int      __stdcall DestroyWindow(HWND);
ani_import longptr  __stdcall DispatchMessageA(MSG *);
ani_import HDC      __stdcall GetDC(HWND);
ani_import HMODULE  __stdcall GetModuleHandleA(char *);
ani_import int      __stdcall GetMonitorInfoA(HMONITOR, MONITORINFO *);
ani_extern voidfunc __stdcall GetProcAddress(HMODULE, char *);
ani_import int      __stdcall GetWindowPlacement(HWND, WINDOWPLACEMENT *);
ani_import HMODULE  __stdcall LoadLibraryA(char *);
ani_import HCURSOR  __stdcall LoadCursorA(HINSTANCE, char *);
ani_import HMONITOR __stdcall MonitorFromWindow(HWND, ulong);
ani_import int      __stdcall PeekMessageA(MSG *, HWND, uint, uint, uint);
ani_import int      __stdcall QueryPerformanceCounter(LARGE_INTEGER *);
ani_import int      __stdcall QueryPerformanceFrequency(LARGE_INTEGER *);
ani_import int      __stdcall ReleaseDC(HWND, HDC);
ani_import ushort   __stdcall RegisterClassExA(WNDCLASSEXA *);
ani_import int      __stdcall SetPixelFormat(HDC, int, PIXELFORMATDESCRIPTOR *);
ani_import int      __stdcall SetWindowPlacement(HWND, WINDOWPLACEMENT *);
ani_import int      __stdcall SetWindowPos(HWND, HWND, int, int, int, int, uint);
ani_import int      __stdcall ShowWindow(HWND, int);
ani_import int      __stdcall SwapBuffers(HDC);
ani_import int      __stdcall TranslateMessage(MSG *);
ani_import void *   __stdcall VirtualAlloc(void *p, usize size, ulong type, ulong protect);
ani_import i32      __stdcall VirtualFree(void *p, ulong size, ulong type);
ani_import HGLRC    __stdcall wglCreateContext(HDC);
ani_import int      __stdcall wglDeleteContext(HGLRC);
ani_import voidfunc __stdcall wglGetProcAddress(char *);
ani_import int      __stdcall wglMakeCurrent(HDC, HGLRC);

#ifdef _WIN64
    ani_import longptr __stdcall GetWindowLongPtrA(HWND, int);
    ani_import longptr __stdcall SetWindowLongPtrA(HWND, int, longptr);
#else
    ani_import longptr __stdcall GetWindowLongA(HWND, int);
    ani_import long __stdcall SetWindowLongA(HWND, int, long);
    
    #define GetWindowLongPtrA GetWindowLongA
    #define SetWindowLongPtrA SetWindowLongA
#endif

#define WIN32_ANI_WINDOWS_H
#endif
