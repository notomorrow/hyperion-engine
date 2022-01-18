
#version 330 core
precision highp float;

#include "../include/frag_output.inc"

uniform sampler2D ColorMap;

const float BloomSpread = 0.8;
const float BloomIntensity = 1.1;

in vec2 v_texcoord0;

void main(void)
{
    ivec2 size = textureSize(ColorMap, 0);

    float uv_x = v_texcoord0.x * size.x;
    float uv_y = v_texcoord0.y * size.y;

    vec4 sum = vec4(0.0);
    for (int n = 0; n < 9; ++n) {
        uv_y = (v_texcoord0.y * size.y) + (BloomSpread * float(n - 4));
        vec4 h_sum = vec4(0.0);
        h_sum += texelFetch(ColorMap, ivec2(uv_x - (4.0 * BloomSpread), uv_y), 0);
        h_sum += texelFetch(ColorMap, ivec2(uv_x - (3.0 * BloomSpread), uv_y), 0);
        h_sum += texelFetch(ColorMap, ivec2(uv_x - (2.0 * BloomSpread), uv_y), 0);
        h_sum += texelFetch(ColorMap, ivec2(uv_x - BloomSpread, uv_y), 0);
        h_sum += texelFetch(ColorMap, ivec2(uv_x, uv_y), 0);
        h_sum += texelFetch(ColorMap, ivec2(uv_x + BloomSpread, uv_y), 0);
        h_sum += texelFetch(ColorMap, ivec2(uv_x + (2.0 * BloomSpread), uv_y), 0);
        h_sum += texelFetch(ColorMap, ivec2(uv_x + (3.0 * BloomSpread), uv_y), 0);
        h_sum += texelFetch(ColorMap, ivec2(uv_x + (4.0 * BloomSpread), uv_y), 0);
        sum += h_sum / 9.0;
    }

    vec3 color = (texture(ColorMap, v_texcoord0) + ((sum / 9.0) * BloomIntensity)).rgb;

    //Tonemapping and color grading
    color = pow(color, vec3(1.5));
    color = color / (1.0 + color);
    color = pow(color, vec3(1.0 / 1.5));

    
    color = mix(color, color * color * (3.0 - 2.0 * color), vec3(1.0));
    color = pow(color, vec3(1.3, 1.20, 1.0));    

	  color = clamp(color * 1.01, 0.0, 1.0);

    output0 = vec4(color, 1.0);
}
