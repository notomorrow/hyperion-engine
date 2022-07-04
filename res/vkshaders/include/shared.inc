#ifndef HYP_SHARED_GLSL
#define HYP_SHARED_GLSL

vec4 Texture2D(sampler2D tex, vec2 texcoord)
{
#if HYP_GBUFFER_FLIP_Y
    return texture(tex, vec2(texcoord.s, 1.0 - texcoord.t));
#else
    return texture(tex, texcoord);
#endif
}

vec4 Texture2DLod(sampler2D tex, vec2 texcoord, float lod)
{
#if HYP_GBUFFER_FLIP_Y
    return texture(tex, vec2(texcoord.s, 1.0 - texcoord.t), lod);
#else
    return texture(tex, texcoord, lod);
#endif
}

vec4 Texture2D(sampler samp, texture2D tex, vec2 texcoord)
{
#if HYP_GBUFFER_FLIP_Y
    return texture(sampler2D(tex, samp), vec2(texcoord.s, 1.0 - texcoord.t));
#else
    return texture(sampler2D(tex, samp), texcoord);
#endif
}

vec4 Texture2DLod(sampler samp, texture2D tex, vec2 texcoord, float lod)
{
#if HYP_GBUFFER_FLIP_Y
    return texture(sampler2D(tex, samp), vec2(texcoord.s, 1.0 - texcoord.t), lod);
#else
    return texture(sampler2D(tex, samp), texcoord, lod);
#endif
}

vec4 TextureCube(samplerCube tex, vec3 texcoord)
{
    return texture(tex, texcoord);
}

vec4 TextureCubeLod(samplerCube tex, vec3 texcoord, float lod)
{
    return texture(tex, texcoord, lod);
}

vec4 TextureCube(sampler samp, textureCube tex, vec3 texcoord)
{
    return texture(samplerCube(tex, samp), texcoord);
}

vec4 TextureCubeLod(sampler samp, textureCube tex, vec3 texcoord, float lod)
{
    return texture(samplerCube(tex, samp), texcoord, lod);
}

float LinearDepth(mat4 projection, float depth)
{
    return projection[3][2] / (depth * projection[2][3] - projection[2][2]);
}

vec4 ReconstructPositionFromDepth(mat4 inverse_view_projection, vec2 coord, float depth)
{
    vec4 ndc = vec4(coord * 2.0 - 1.0, depth, 1.0);

    vec4 inversed = inverse_view_projection * ndc;
    inversed /= inversed.w;
    inversed.w = 1.0;

    return inversed;
}


#endif
