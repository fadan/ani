#ifndef ANI_TYPES_H

#if defined(__cplusplus)
    #define ani_extern  extern "C"
#else
    #define ani_extern  extern
#endif

#define ani_import  ani_extern __declspec(dllimport)
#define ani_export  ani_extern __declspec(dllexport)

//
// NOTE(dan): platforms
//

#define PLATFORM_UNDEFINED  0
#define PLATFORM_WINDOWS    1
#define PLATFORM_UNIX       2
#define PLATFORM_OSX        3

#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM    PLATFORM_WINDOWS
#elif defined(__unix__)
    #define PLATFORM    PLATFORM_UNIX
#elif defined(__APPLE__) && defined(__MACH__)
    #define PLATFORM    PLATFORM_OSX
#else
    #define PLATFORM    PLATFORM_UNDEFINED
#endif

//
// NOTE(dan): compilers
//

#define COMPILER_UNDEFINED  0
#define COMPILER_MSVC       1
#define COMPILER_GCC        2
#define COMPILER_CLANG      3

#if defined(_MSC_VER)
    #define COMPILER    COMPILER_MSVC
#elif defined(__GNUC__)
    #define COMPILER    COMPILER_GCC
#elif defined(__clang__)
    #define COMPILER    COMPILER_CLANG
#else
    #define COMPILER    COMPILER_UNDEFINED
#endif

#define ARCH_32_BIT    1
#define ARCH_64_BIT    2

#if defined(_WIN64) || defined(__x86_64__) || defined(__ia64__) || defined(__LP64__)
    #define ARCH    ARCH_64_BIT
#else
    #define ARCH    ARCH_32_BIT
#endif

//
// NOTE(dan): types
//

typedef unsigned char    u8;
typedef   signed char    i8;
typedef unsigned short  u16;
typedef   signed short  i16;
typedef unsigned int    u32;
typedef   signed int    i32;

#if COMPILER == COMPILER_MSVC
    typedef unsigned __int64    u64;
    typedef          __int64    i64;
#else
    typedef unsigned long long  u64;
    typedef          long long  i64;
#endif

typedef u8             uchar;
typedef u16            ushort;
typedef u32            uint;
typedef unsigned long  ulong;

typedef i32    b32;
typedef float  f32;
typedef double f64;

#if ARCH == ARCH_64_BIT
    typedef u64     usize;
    typedef i64     isize;
    typedef u64     uintptr;
    typedef i64     intptr;
    typedef u64     ulongptr;
    typedef i64     longptr;
#else
    typedef u32     usize;
    typedef i32     isize;
    typedef u32     uintptr;
    typedef i32     intptr;
    typedef ulong   ulongptr;
    typedef long    longptr;
#endif

typedef char test_size_u8[sizeof(u8) == 1 ? 1 : -1];
typedef char test_size_u16[sizeof(u16) == 2 ? 1 : -1];
typedef char test_size_u32[sizeof(u32) == 4 ? 1 : -1];
typedef char test_size_u64[sizeof(u64) == 8 ? 1 : -1];
typedef char test_size_usize[sizeof(usize) == sizeof(char *) ? 1 : -1];

//
// NOTE(dan): min/max values
//

#define  U8_MAX 0xff
#define U16_MAX 0xffff
#define U32_MAX 0xffffffff
#define U64_MAX 0xffffffffffffffff

#define  I8_MAX 0x7f
#define I16_MAX 0x7fff
#define I32_MAX 0x7fffffff
#define I64_MAX 0x7fffffffffffffff

#define  I8_MIN ( -I8_MAX - 1)
#define I16_MIN (-I16_MAX - 1)
#define I32_MIN (-I32_MAX - 1)

#define F32_MAX (1E+37)
#define F32_MIN (1E-37)

#define PI32  3.1415926535897932384626433832795f
#define TAU32 6.2831853071795864769252867665590f

#define ANI_TYPES_H
#endif
