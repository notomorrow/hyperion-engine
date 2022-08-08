#ifndef HYP_NOISE_GLSL
#define HYP_NOISE_GLSL
float rand(vec2 n)
{ 
    return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float SimpleNoise(vec2 n)
{
    const vec2 d = vec2(0.0, 1.0);
    vec2 b = floor(n), f = smoothstep(vec2(0.0), vec2(1.0), fract(n));
    return mix(mix(rand(b), rand(b + d.yx), f.x), mix(rand(b + d.xy), rand(b + d.yy), f.x), f.y);
}

vec2 VogelDisk(int sample_index, int num_samples, float phi)
{
    const float golden_angle = 2.4;

    float r = sqrt(float(sample_index) + 0.5) / sqrt(float(num_samples));
    float theta = sample_index * golden_angle + phi;

    return vec2(r * cos(theta), r * sin(theta));
}

float InterleavedGradientNoise(vec2 position_screen)
{
    const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);

    return fract(magic.z * fract(dot(position_screen, magic.xy)));
}


#endif