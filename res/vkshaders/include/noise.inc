#ifndef HYP_NOISE_GLSL
#define HYP_NOISE_GLSL

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