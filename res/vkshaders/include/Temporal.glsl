#ifndef TEMPORAL_GLSL
#define TEMPORAL_GLSL

const vec2 neighbor_uv_offsets_3x3[9] = {
    vec2(-1.0, -1.0),
    vec2(0.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 0.0),
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(-1.0, 1.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
};

vec4 AdjustColorIn(in vec4 color)
{
    // watch out for NaN
    return vec4(log(color.rgb), color.a);
}

vec4 AdjustColorOut(in vec4 color)
{
    // watch out for NaN
    return vec4(exp(color.rgb), color.a);
}

void GetPixelNeighbors_3x3(in texture2D tex, in vec2 uv, in vec2 texel_size, out vec4 neighbors[9])
{
    vec2 offset_uv;

    for (uint i = 0; i < 9; i++) {
        offset_uv = uv + (neighbor_uv_offsets_3x3[i] * texel_size);

        vec4 neighbor_color = AdjustColorIn(Texture2DLod(sampler_nearest, tex, offset_uv, 0.0));

        neighbors[i] = neighbor_color;
    }
}

void GetPixelNeighborsMinMax_3x3(in texture2D tex, in vec2 uv, in vec2 texel_size, out vec4 min_value, out vec4 max_value)
{
    vec4 _min_value = vec4(1000000.0);
    vec4 _max_value = vec4(-1000000.0);

    vec2 offset_uv;

    for (uint i = 0; i < 9; i++) {
        offset_uv = uv + (neighbor_uv_offsets_3x3[i] * texel_size);

        vec4 neighbor_color = AdjustColorIn(Texture2DLod(sampler_nearest, tex, offset_uv, 0.0));

        _min_value = min(_min_value, neighbor_color);
        _max_value = max(_max_value, neighbor_color);
    }

    min_value = _min_value;
    max_value = _max_value;
}

vec4 MinColors_3x3(in vec4 colors[9])
{
    vec4 result = colors[0];

    for (uint i = 1; i < 9; i++) {
        result = min(result, colors[i]);
    }

    return result;
}

vec4 MaxColors_3x3(in vec4 colors[9])
{
    vec4 result = colors[0];

    for (uint i = 1; i < 9; i++) {
        result = max(result, colors[i]);
    }

    return result;
}

vec4 ClampColor_3x3(in texture2D tex, in vec4 previous_value, in vec2 uv, in vec2 texel_size)
{
    vec4 min_value, max_value;
    GetPixelNeighborsMinMax_3x3(tex, uv, texel_size, min_value, max_value);

    return AdjustColorOut(clamp(AdjustColorIn(previous_value), min_value, max_value));
}

#endif