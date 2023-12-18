#ifndef LIGHT_FIELD_SHARED_GLSL
#define LIGHT_FIELD_SHARED_GLSL

#include "../include/Octahedron.glsl"

#define PROBE_SIDE_LENGTH 256
#define PROBE_SIDE_LENGTH_IRRADIANCE 64
#define PROBE_SIDE_LENGTH_LOWRES 16
#define PROBE_BORDER_LENGTH 2
#define PROBE_SIDE_LENGTH_BORDER_IRRADIANCE (PROBE_SIDE_LENGTH_IRRADIANCE + PROBE_BORDER_LENGTH)
#define PROBE_SIDE_LENGTH_BORDER (PROBE_SIDE_LENGTH + PROBE_BORDER_LENGTH)

struct Ray
{
    vec3 origin;
    vec3 direction;
};

int idot(ivec3 a, ivec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float DistanceSquared(vec3 a, vec3 b)
{
    return dot(a - b, a - b);
}

float DistanceSquared(vec2 a, vec2 b)
{
    return dot(a - b, a - b);
}

float LengthSquared(vec3 v)
{
    return dot(v, v);
}

float LengthSquared(vec2 v)
{
    return dot(v, v);
}

float MaxComponent(vec4 v)
{
    return max(max(max(v.x, v.y), v.z), v.w);
}

float MaxComponent(vec3 v)
{
    return max(max(v.x, v.y), v.z);
}

float MaxComponent(vec2 v)
{
    return max(v.x, v.y);
}

/** Two-element sort: maybe swaps a and b so that a' = min(a, b), b' = max(a, b). */
void MinSwap(inout float a, inout float b)
{
    float temp = min(a, b);
    b = max(a, b);
    a = temp;
}

/** Sort the three values in v from least to greatest using an exchange network (i.e., no branches) */
void Sort(inout vec3 v)
{
    MinSwap(v[0], v[1]);
    MinSwap(v[1], v[2]);
    MinSwap(v[0], v[1]);
}

#endif