#version 330 core
precision highp float;
#include "../include/frag_output.inc"


/*uniform sampler2D ColorMap; //Image to be processed
uniform sampler2D DepthMap; //Linear depth, where 1.0 == far plane
uniform vec2 PixelSize; //The size of a pixel: vec2(1.0/width, 1.0/height)
uniform vec2 CameraNearFar;
uniform float FocusScale;
uniform float FocusRange;
in vec2 v_texcoord0;

const float GOLDEN_ANGLE = 2.39996323;
const float MAX_BLUR_SIZE = 10.0;
const float RAD_SCALE = 1.5; // Smaller = nicer blur, larger = faster

float getBlurSize(float depth, float focusPoint, float focusScale)
{
 float coc = clamp((1.0 / focusPoint - 1.0 / depth)*focusScale, -1.0, 1.0);
 return abs(coc) * MAX_BLUR_SIZE;
}

vec3 depthOfField(vec2 texCoord, float focusPoint, float focusScale)
{
 float centerDepth = texture(DepthMap, texCoord).r * CameraNearFar.y;
 float centerSize = getBlurSize(centerDepth, focusPoint, focusScale);
 vec3 color = texture(ColorMap, v_texcoord0).rgb;
 float tot = 1.0;

 float radius = RAD_SCALE;
 for (float ang = 0.0; radius<MAX_BLUR_SIZE; ang += GOLDEN_ANGLE)
 {
  vec2 tc = texCoord + vec2(cos(ang), sin(ang)) * PixelSize * radius;

  vec3 sampleColor = texture(ColorMap, tc).rgb;
  float sampleDepth = texture(DepthMap, tc).r * CameraNearFar.y;
  float sampleSize = getBlurSize(sampleDepth, focusPoint, focusScale);
  if (sampleDepth > centerDepth)
   sampleSize = clamp(sampleSize, 0.0, centerSize*2.0);

  float m = smoothstep(radius-0.5, radius+0.5, sampleSize);
  color += mix(color/tot, sampleColor, m);
  tot += 1.0;
  radius += RAD_SCALE/radius;
 }
 return color /= tot;
}

void main()
{
    output0 = vec4(depthOfField(v_texcoord0, FocusRange, FocusScale), 1.0);
}*/

uniform sampler2D ColorMap;
uniform sampler2D DepthMap;
uniform vec2 CameraNearFar;
uniform vec2 Scale;
uniform float FocusRange;

in vec2 v_texcoord0;
void main()                                  
{                                            
    float depth = texture(DepthMap, v_texcoord0).r;

    vec4 finalBlurVal = vec4(0.0, 0.0, 0.0, 1.0);
    finalBlurVal += texture( ColorMap, v_texcoord0 + vec2( -3.0*Scale.x, -3.0*Scale.y ) ) * 0.015625;
    finalBlurVal += texture( ColorMap, v_texcoord0 + vec2( -2.0*Scale.x, -2.0*Scale.y ) )*0.09375;
    finalBlurVal += texture( ColorMap, v_texcoord0 + vec2( -1.0*Scale.x, -1.0*Scale.y ) )*0.234375;
    finalBlurVal += texture( ColorMap, v_texcoord0 + vec2( 0.0 , 0.0) )*0.3125;
    finalBlurVal += texture( ColorMap, v_texcoord0 + vec2( 1.0*Scale.x,  1.0*Scale.y ) )*0.234375;
    finalBlurVal += texture( ColorMap, v_texcoord0 + vec2( 2.0*Scale.x,  2.0*Scale.y ) )*0.09375;
    finalBlurVal += texture( ColorMap, v_texcoord0 + vec2( 3.0*Scale.x, -3.0*Scale.y ) ) * 0.015625;

    float a = CameraNearFar.y / (CameraNearFar.y - CameraNearFar.x);  
    float b = CameraNearFar.y * CameraNearFar.x / (CameraNearFar.x - CameraNearFar.y);
    float z = b / (depth - a);
    float dynamicDepth = b / (depth - a);
    float unfocus = min( 1.0, abs( z - 1.0 ) / FocusRange );

    output0 = mix(texture(ColorMap, v_texcoord0), finalBlurVal, unfocus);  
}

