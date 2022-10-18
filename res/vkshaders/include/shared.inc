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
    return textureLod(tex, vec2(texcoord.s, 1.0 - texcoord.t), lod);
#else
    return textureLod(tex, texcoord, lod);
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
    return textureLod(sampler2D(tex, samp), vec2(texcoord.s, 1.0 - texcoord.t), lod);
#else
    return textureLod(sampler2D(tex, samp), texcoord, lod);
#endif
}

vec4 Texture2DTexel(sampler2D tex, ivec2 coord)
{
    return texelFetch(tex, coord, 0);
}

vec4 Texture2DTexel(sampler samp, texture2D tex, ivec2 coord)
{
    return texelFetch(sampler2D(tex, samp), coord, 0);
}

vec4 Texture2DTexelLod(sampler samp, texture2D tex, ivec2 coord, int lod)
{
    return texelFetch(sampler2D(tex, samp), coord, lod);
}

vec4 Texture2DGather(sampler2D tex, vec2 coord)
{
    return textureGather(tex, coord);
}

vec4 Texture3D(sampler samp, texture3D tex, vec3 texcoord)
{
    return texture(sampler3D(tex, samp), texcoord);
}

vec4 Texture3DLod(sampler samp, texture3D tex, vec3 texcoord, float lod)
{
    return textureLod(sampler3D(tex, samp), texcoord, lod);
}

vec4 Texture3DTexel(sampler3D tex, ivec3 coord)
{
    return texelFetch(tex, coord, 0);
}

vec4 Texture3DTexel(sampler samp, texture3D tex, ivec3 coord)
{
    return texelFetch(sampler3D(tex, samp), coord, 0);
}

vec4 Texture3DTexelLod(sampler samp, texture3D tex, ivec3 coord, int lod)
{
    return texelFetch(sampler3D(tex, samp), coord, lod);
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

float ViewDepth(float depth, float near, float far)
{
   /* float x = 1.0 - (far / near);
    float y = (far / near);

    return 1.0 / ((x / far) * depth + (y / far));*/
    
    return (far * near) / (far - depth * (far - near));
}

vec4 ReconstructWorldSpacePositionFromDepth(mat4 inverse_projection, mat4 inverse_view, vec2 coord, float depth)
{
    vec4 ndc = vec4(coord * 2.0 - 1.0, depth, 1.0);

    vec4 inversed = inverse_projection * ndc;
    inversed /= inversed.w;
    inversed = inverse_view * inversed;

    return inversed;
}

vec4 ReconstructViewSpacePositionFromDepth(mat4 inverse_projection, vec2 coord, float depth)
{
    vec4 ndc = vec4(coord * 2.0 - 1.0, depth, 1.0);

    vec4 inversed = inverse_projection * ndc;
    inversed /= inversed.w;
    inversed.w = 1.0;

    return inversed;
}

vec2 GetProjectedPosition(in mat4 projection, in vec3 view_space_position)
{
	vec4 sample_position = projection * vec4(view_space_position, 1.0);
	return (sample_position.xy / sample_position.w) * 0.5 + 0.5;
}

float Saturate(float val)
{
    return min(max(val, 0.0), 1.0);
}

vec2 Saturate(vec2 val)
{
    return min(max(val, vec2(0.0)), vec2(1.0));
}

vec3 Saturate(vec3 val)
{
    return min(max(val, vec3(0.0)), vec3(1.0));
}

vec4 Saturate(vec4 val)
{
    return min(max(val, vec4(0.0)), vec4(1.0));
}

vec3 GetTriplanarBlend(vec3 normal)
{
    vec3 blending = normalize(max(abs(normal), 0.0001));
    blending /= (blending.x + blending.y + blending.z);

    return blending;
}

vec4 Texture2DTriplanar(sampler samp, texture2D tex, vec3 position, vec3 normal)
{
    vec3 blending = GetTriplanarBlend(normal);

    vec4 sample_x = Texture2D(samp, tex, position.zy * 0.01);
    vec4 sample_y = Texture2D(samp, tex, position.xz * 0.01);
    vec4 sample_z = Texture2D(samp, tex, position.xy * 0.01);
    
    return sample_x * blending.x + sample_y * blending.y + sample_z * blending.z;
}

vec4 Texture2DTriplanar(sampler2D tex, vec3 position, vec3 normal)
{
    vec3 blending = GetTriplanarBlend(normal);

    vec4 sample_x = Texture2D(tex, position.zy * 0.01);
    vec4 sample_y = Texture2D(tex, position.xz * 0.01);
    vec4 sample_z = Texture2D(tex, position.xy * 0.01);

    return sample_x * blending.x + sample_y * blending.y + sample_z * blending.z;
}

vec4 CalculateFogExp(in vec4 start_color, in vec4 end_color, vec3 world_position, vec3 camera_position, float fog_start, float fog_end)
{
    const float dist = distance(world_position, camera_position);
    const float density = 0.00003;

    const float fog_factor = 1.0 / exp(dist * density);

    return mix(start_color, end_color, 1.0 - fog_factor);
}

vec4 CalculateFogLinear(in vec4 start_color, in vec4 end_color, vec3 world_position, vec3 camera_position, float fog_start, float fog_end)
{
    const float dist = distance(world_position, camera_position);

    const float fog_factor = Saturate((fog_end - dist) / (fog_end - fog_start));

    return mix(start_color, end_color, 1.0 - fog_factor);
}

float RoughnessToConeAngle(float roughness)
{
	float specular_power = 2.0 / pow(roughness, 4.0) - 2.0;

    const float xi = 0.244;
    float exponent = 1.0 / (specular_power + 1.0);
    return acos(pow(xi, exponent));
}

int GetNumLevels(sampler samp, texture2D tex)
{
    return textureQueryLevels(sampler2D(tex, samp));
}

int GetNumLevels(sampler samp, texture3D tex)
{
    return textureQueryLevels(sampler3D(tex, samp));
}

int GetNumLevels(sampler samp, textureCube tex)
{
    return textureQueryLevels(samplerCube(tex, samp));
}

#endif
