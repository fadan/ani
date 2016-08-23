#ifndef ANI_MATH_H

// TODO(dan): remove this
#include <math.h>

#define deg2rad(a)  ((a) * (PI32 / 180))

union mat4
{
    f32 data[16];
    struct
    {
        f32 m00, m01, m02, m03;
        f32 m10, m11, m12, m13;
        f32 m20, m21, m22, m23;
        f32 m30, m31, m32, m33;
    };
};

typedef union
{
    f32 data[3];
    struct
    {
        f32 x, y, z;
    };
    struct
    {
        f32 r, g, b;
    };
    struct 
    {
        f32 u, v, w;
    };
} vec3;

inline void mat4_set_zero(mat4 *mat)
{
    memset(mat, 0, sizeof(*mat));
}

inline void mat4_set_identity(mat4 *mat)
{
    memset(mat, 0, sizeof(*mat));
    mat->m00 = mat->m11 = mat->m22 = mat->m33 = 1.0f;
}

inline void mat4_multiply(mat4 *left, mat4 *right)
{
    mat4 m;
    for (u32 i = 0; i < 4; ++i)
    {
        m.data[i*4 + 0] = ((left->data[i*4 + 0] * right->data[0*4 + 0]) +
                           (left->data[i*4 + 1] * right->data[1*4 + 0]) +
                           (left->data[i*4 + 2] * right->data[2*4 + 0]) +
                           (left->data[i*4 + 3] * right->data[3*4 + 0]));
        m.data[i*4 + 1] = ((left->data[i*4 + 0] * right->data[0*4 + 1]) +
                           (left->data[i*4 + 1] * right->data[1*4 + 1]) +
                           (left->data[i*4 + 2] * right->data[2*4 + 1]) +
                           (left->data[i*4 + 3] * right->data[3*4 + 1]));
        m.data[i*4 + 2] = ((left->data[i*4 + 0] * right->data[0*4 + 2]) +
                           (left->data[i*4 + 1] * right->data[1*4 + 2]) +
                           (left->data[i*4 + 2] * right->data[2*4 + 2]) +
                           (left->data[i*4 + 3] * right->data[3*4 + 2]));
        m.data[i*4 + 3] = ((left->data[i*4 + 0] * right->data[0*4 + 3]) +
                           (left->data[i*4 + 1] * right->data[1*4 + 3]) +
                           (left->data[i*4 + 2] * right->data[2*4 + 3]) +
                           (left->data[i*4 + 3] * right->data[3*4 + 3]));
    }
    memcpy(left, &m, sizeof(m));
}

inline void mat4_set_translation(mat4 *mat, f32 x, f32 y, f32 z)
{
    mat4_set_identity(mat);
    mat->m30 = x;
    mat->m31 = y;
    mat->m32 = z;
}

inline void mat4_translate(mat4 *mat, f32 x, f32 y, f32 z)
{
    mat4 m;
    mat4_set_translation(&m, x, y, z);
    mat4_multiply(mat, &m);
}

inline void mat4_set_scaling(mat4 *mat, f32 x, f32 y, f32 z)
{
    mat4_set_identity(mat);
    mat->m00 = x;
    mat->m11 = y;
    mat->m22 = z;
}

inline void mat4_scale(mat4 *mat, f32 x, f32 y, f32 z)
{
    mat4 m;
    mat4_set_scaling(&m, x, y, z);
    mat4_multiply(mat, &m);
}

inline void mat4_set_rotation(mat4 *mat, f32 angle, f32 x, f32 y, f32 z)
{
    f32 c, s, norm;

    c = cosf(PI32 * angle / 180.0f);
    s = sinf(PI32 * angle / 180.0f);
    norm = sqrtf(x * x + y * y + z * z);

    x /= norm;
    y /= norm;
    z /= norm;

    mat4_set_identity(mat);

    mat->m00 = x * x * (1 - c) + c;
    mat->m10 = y * x * (1 - c) - z * s;
    mat->m20 = z * x * (1 - c) + y * s;

    mat->m01 = x * y * (1 - c) + z * s;
    mat->m11 = y * y * (1 - c) + c;
    mat->m21 = z * y * (1 - c) - x * s;

    mat->m02 = x * z * (1 - c) - y * s;
    mat->m12 = y * z * (1 - c) + x * s;
    mat->m22 = z * z * (1 - c) + c;
}

inline void mat4_rotate(mat4 *mat, f32 angle, f32 x, f32 y, f32 z)
{
    mat4 m;
    mat4_set_rotation(&m, angle, x, y, z);
    mat4_multiply(mat, &m);
}

#define ANI_MATH_H
#endif
