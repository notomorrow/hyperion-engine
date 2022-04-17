#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;

// TODO: read in from prev pass.

layout(location=0) out vec4 color_output;

#include "include/gbuffer.inc"
#include "include/scene.inc"

const float diffarea = 0.3; //self-shadowing reduction
const float gdisplace = 0.4; //gauss bell center //0.4

vec2 texcoord = vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

#define PI 3.14159265359
#define SSAO_NOISE 1
#define SSAO_NOISE_AMOUNT 0.0002
#define SSAO_MIST 1
#define SSAO_MIST_START 0.0
#define SSAO_MIST_END 0.01
#define CAP_MIN_DISTANCE 0.0001
#define CAP_MAX_DISTANCE 0.01
#define SSAO_SAMPLES 64
#define SSAO_STRENGTH 1.0
#define SSAO_CLAMP_AMOUNT 0.125
#define SSAO_RADIUS 7.0

vec2 GetNoise(vec2 coord) //generating noise/pattern texture for dithering
{
    float width = float(scene.resolution_x);
    float height = float(scene.resolution_y);
    
    float noise_x = ((fract(1.0 - coord.s * (width / 2.0)) * 0.25) + (fract(coord.t * (height / 2.0)) * 0.75)) * 2.0 - 1.0;
    float noise_y = ((fract(1.0 - coord.s * (width / 2.0)) * 0.75) + (fract(coord.t * (height / 2.0)) * 0.25)) * 2.0 - 1.0;

#if SSAO_NOISE
    noise_x = clamp(fract(sin(dot(coord, vec2(12.9898, 78.233))) * 43758.5453), 0.0, 1.0) * 2.0 - 1.0;
    noise_y = clamp(fract(sin(dot(coord, vec2(12.9898, 78.233) * 2.0)) * 43758.5453), 0.0, 1.0) * 2.0 - 1.0;
#endif

    return vec2(noise_x, noise_y) * SSAO_NOISE_AMOUNT;
}

#if SSAO_MIST
float CalculateMist()
{
    float depth = texture(gbuffer_depth_texture, texcoord).r;
    float depth_mist = -CAP_MAX_DISTANCE * CAP_MIN_DISTANCE / (depth * (CAP_MAX_DISTANCE - CAP_MIN_DISTANCE) - CAP_MAX_DISTANCE);
    
    return clamp((depth_mist - SSAO_MIST_START) / SSAO_MIST_END, 0.0, 1.0);
}
#endif

float ReadDepth(vec2 coord)
{
    if (texcoord.x < 0.0 || texcoord.y < 0.0) {
        return 1.0;
    } else {
        float depth = texture(gbuffer_depth_texture, coord).r;
        float z_n = 2.0 * depth - 1.0;
        
        return (2.0 * CAP_MIN_DISTANCE) / (CAP_MAX_DISTANCE + CAP_MIN_DISTANCE - z_n * (CAP_MAX_DISTANCE - CAP_MIN_DISTANCE));
    }
}

int CompareDepthsFar(float depth1, float depth2)
{
    float garea = 2.0; //gauss bell width
    float diff = (depth1 - depth2) * 100.0; //depth difference (0-100)
    //reduce left bell width to avoid self-shadowing
    return int(diff >= gdisplace);
}

float CompareDepths(float depth1, float depth2)
{
    float diff = (depth1 - depth2) * 100.0; //depth difference (0-100)
    //reduce left bell width to avoid self-shadowing
    float garea = (diff < gdisplace) ? diffarea : 2.0; // gauss bell width

    return pow(2.7182, -2.0 * (diff - gdisplace) * (diff - gdisplace) / (garea * garea));
}

float CalculateAO(float depth, float dw, float dh)
{
    float dd = (1.0-depth)*SSAO_RADIUS;

    float temp = 0.0;
    float temp2 = 0.0;
    float coordw = texcoord.x + dw*dd;
    float coordh = texcoord.y + dh*dd;
    float coordw2 = texcoord.x - dw*dd;
    float coordh2 = texcoord.y - dh*dd;

    vec2 coord = vec2(coordw, coordh);
    vec2 coord2 = vec2(coordw2, coordh2);

    float cd = ReadDepth(coord);
    int far = CompareDepthsFar(depth, cd);
    temp = CompareDepths(depth, cd);
    //DEPTH EXTRAPOLATION:
    if (far > 0) {
        temp2 = CompareDepths(ReadDepth(coord2), depth);
        temp += (1.0-temp)*temp2;
    }

    return temp;
}

void main()
{
    float width = float(scene.resolution_x);
    float height = float(scene.resolution_y);

    vec2 noise = GetNoise(texcoord);
    float depth = ReadDepth(texcoord);

    float w = (1.0 / width) / clamp(depth, SSAO_CLAMP_AMOUNT, 1.0) + (noise.x * (1.0 - noise.x));
    float h = (1.0 / height) / clamp(depth, SSAO_CLAMP_AMOUNT, 1.0) + (noise.y * (1.0 - noise.y));

    float pw = 0.0;
    float ph = 0.0;

    float ao = 0.0;

    float dl = PI * (3.0 - sqrt(5.0));
    float dz = 1.0 / float(SSAO_SAMPLES);
    float l = 0.0;
    float z = 1.0 - dz/2.0;

    for (int i = 0; i < SSAO_SAMPLES; i++) {
        float r = sqrt(1.0 - z);

        pw = cos(l) * r;
        ph = sin(l) * r;
        ao += CalculateAO(depth, pw * w, ph * h);
        z = z - dz;
        l = l + dl;
    }

    ao /= float(SSAO_SAMPLES);
    ao *= SSAO_STRENGTH;
    ao = 1.0-ao;

#if SSAO_MIST
    ao = mix(ao, 1.0, CalculateMist());
#endif
    
    color_output = vec4(ao, 0.0, 0.0, 1.0);
}