#version 330 core

#include "../include/frag_output.inc"

uniform sampler2D NormalMap;
uniform sampler2D DepthMap;
uniform sampler2D ColorMap;
uniform sampler2D u_noiseMap;
uniform vec2 u_resolution;

uniform mat4 u_inverseProjectionMatrix;

in vec2 v_texcoord0;

#define $SPREAD 1.2

vec3 readNormal( in vec2 coord) {
  return normalize(texture(NormalMap, coord).xyz * 2.0 - 1.0);
}
vec3 posFromDepth(vec2 coord) {
  float d = texture(DepthMap, coord).r;
  vec3 tray = mat3x3(u_inverseProjectionMatrix) * vec3((coord.x - 0.5) * 2.0, (coord.y - 0.5) * 2.0, 1.0);
  return tray * d;
} //Ambient Occlusion form factor:
float aoFF( in vec3 ddiff, in vec3 cnorm, in float c1, in float c2) {
  vec3 vv = normalize(ddiff);
  float rd = length(ddiff);
  return (1.0 - clamp(dot(readNormal(v_texcoord0 + vec2(c1, c2)), -vv), 0.0, 1.0)) * clamp(dot(cnorm, vv), 0.0, 1.0) * (1.0 - 1.0 / sqrt(1.0 / (rd * rd) + 1.0));
} //GI form factor:
float giFF( in vec3 ddiff, in vec3 cnorm, in float c1, in float c2) {
  vec3 vv = normalize(ddiff);
  float rd = length(ddiff);
  return 1.0 * clamp(dot(readNormal(v_texcoord0 + vec2(c1, c2)), -vv), 0.0, 1.0) * clamp(dot(cnorm, vv), 0.0, 1.0) / (rd * rd + 1.0);
}
void main() { //read current normal,position and color
  vec3 n = readNormal(v_texcoord0.st);
  vec3 p = posFromDepth(v_texcoord0.st);
  vec3 col = texture(ColorMap, v_texcoord0).rgb; //randomization texture
  vec2 fres = vec2(u_resolution.x / 128.0 * 5, u_resolution.y / 128.0 * 5);
  vec3 random = texture(u_noiseMap, v_texcoord0.st * fres.xy).rgb;
  random = random * 2.0 - vec3(1.0); //initialize variables:
  float ao = 0.0;
  vec3 gi = vec3(0.0,0.0,0.0);
  float incx = 1.0/u_resolution.x*$SPREAD;
  float incy = 1.0/u_resolution.y*$SPREAD;
  float pw = incx;
  float ph = incy;
  float cdepth = texture(DepthMap, v_texcoord0).r;
  //3 rounds of 8 samples each.
  for (float i = 0.0; i < 3.0; ++i) {
    float npw = (pw + 0.0007 * random.x) / cdepth;
    float nph = (ph + 0.0007 * random.y) / cdepth;
    vec3 ddiff = posFromDepth(v_texcoord0.st + vec2(npw, nph)) - p;
    vec3 ddiff2 = posFromDepth(v_texcoord0.st + vec2(npw, -nph)) - p;
    vec3 ddiff3 = posFromDepth(v_texcoord0.st + vec2(-npw, nph)) - p;
    vec3 ddiff4 = posFromDepth(v_texcoord0.st + vec2(-npw, -nph)) - p;
    vec3 ddiff5 = posFromDepth(v_texcoord0.st + vec2(0, nph)) - p;
    vec3 ddiff6 = posFromDepth(v_texcoord0.st + vec2(0, -nph)) - p;
    vec3 ddiff7 = posFromDepth(v_texcoord0.st + vec2(npw, 0)) - p;
    vec3 ddiff8 = posFromDepth(v_texcoord0.st + vec2(-npw, 0)) - p;
    ao += aoFF(ddiff, n, npw, nph);
    ao += aoFF(ddiff2, n, npw, -nph);
    ao += aoFF(ddiff3, n, -npw, nph);
    ao += aoFF(ddiff4, n, -npw, -nph);
    ao += aoFF(ddiff5, n, 0, nph);
    ao += aoFF(ddiff6, n, 0, -nph);
    ao += aoFF(ddiff7, n, npw, 0);
    ao += aoFF(ddiff8, n, -npw, 0);
    gi += giFF(ddiff, n, npw, nph) * texture(ColorMap, v_texcoord0 + vec2(npw, nph)).rgb;
    gi += giFF(ddiff2, n, npw, -nph) * texture(ColorMap, v_texcoord0 + vec2(npw, -nph)).rgb;
    gi += giFF(ddiff3, n, -npw, nph) * texture(ColorMap, v_texcoord0 + vec2(-npw, nph)).rgb;
    gi += giFF(ddiff4, n, -npw, -nph) * texture(ColorMap, v_texcoord0 + vec2(-npw, -nph)).rgb;
    gi += giFF(ddiff5, n, 0, nph) * texture(ColorMap, v_texcoord0 + vec2(0, nph)).rgb;
    gi += giFF(ddiff6, n, 0, -nph) * texture(ColorMap, v_texcoord0 + vec2(0, -nph)).rgb;
    gi += giFF(ddiff7, n, npw, 0) * texture(ColorMap, v_texcoord0 + vec2(npw, 0)).rgb;
    gi += giFF(ddiff8, n, -npw, 0) * texture(ColorMap, v_texcoord0 + vec2(-npw, 0)).rgb;
    //increase sampling area:
    pw += incx;
    ph += incy;
  }
  ao /= 24.0;
  gi /= 24.0;

  output0 = vec4(col, 1.0);
  output4 = vec4(gi * 5.0, 1.0 - ao);
}
