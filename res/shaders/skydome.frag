#version 330 core

#include "include/frag_output.inc"

uniform vec3 v3LightPos;
uniform float fg;
uniform float fg2;
uniform float fExposure;

in vec3 v3Direction;
in vec4 v4RayleighColor;
in vec4 v4MieColor;
in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord0;

uniform vec4 u_sunColor;
uniform float u_globalTime;
uniform sampler2D u_noiseMap;

#if CLOUDS

const float timeScale = 0.5;
const float cloudScale = 0.0008;
const float skyCover = 0.4;
const float softness = 5.0;
const float brightness = 2.0;
const int noiseOctaves = 8;
const float curlStrain = 0.3;

float saturate(float num)
{
  return clamp(num, 0.0, 1.0);
}

float noise(vec2 uv)
{
  return texture(u_noiseMap, uv).r;
}

vec2 rotate(vec2 uv)
{
  uv = uv + noise(uv * 0.2) * 0.005;
  float rot = curlStrain;
  float sinRot = sin(rot);
  float cosRot = cos(rot);
  mat2 rotMat = mat2(cosRot, -sinRot, sinRot, cosRot);
  return uv * rotMat;
}

// generate random texture
float fbm(vec2 uv)
{
  float rot = 1.57;
  float sinRot = sin(rot);
  float cosRot = cos(rot);
  float f = 0.0;
  float total = 0.0;
  float mul = 0.5;
  mat2 rotMat = mat2(cosRot,-sinRot,sinRot,cosRot);
    
  for(int i = 0;i < 8;i++) {
    f += noise(uv + u_globalTime * 0.00015 * (1.0 - mul)) *mul;
    total += mul;
    uv *= 3.0;
    uv = rotate(uv);
    mul *= 0.5;
  }
  return f/total;
}

vec3 getTriPlanarBlend(vec3 _wNorm)
{
  // in wNorm is the world-space normal of the fragment
  vec3 blending = abs(_wNorm);
  blending = normalize(max(blending, 0.00001)); // Force weights to sum to 1.0
  float b = (blending.x + blending.y + blending.z);
  blending /= vec3(b, b, b);
  return blending;
}
  
#endif // CLOUDS

// Mie phase function
float getMiePhase(float fCos, float fCos2, float g, float g2)
{
  return 1.5 * ((1.0 - g2) / (2.0 + g2)) * (1.0 + fCos2) / pow(1.0 + g2 - 2.0*g*fCos, 1.5);
}

// Rayleigh phase function
float getRayleighPhase(float fCos2)
{
  return 0.75 * (2.0 + 0.5 * fCos2); 
}

void main (void)
{
  float fCos = dot(v3LightPos, v3Direction) / length(v3Direction);
  float fCos2 = fCos*fCos;
  vec4 skyColor = v4RayleighColor;
  output0 = skyColor + getMiePhase(fCos, fCos2, fg, fg2) * v4MieColor * u_sunColor;
  //gl_FragColor.a = max(max(gl_FragColor.r, gl_FragColor.g), gl_FragColor.b);
	
	
	
  /*  vec2 uv = v_texCoord0.xy/(400000.0*cloudScale);
    
    float cover = 0.5;
    float bright = 0.85;
    
    float color1 =  fbm(uv-0.5+u_globalTime*0.004*timeScale);
    float color2 = fbm(uv-10.5+u_globalTime*0.002*timeScale);
    
    float clouds1 = smoothstep(1.0-cover,min((1.0-cover)+softness*2.0,1.0),color1);
    float clouds2 = smoothstep(1.0-cover,min((1.0-cover)+softness,1.0),color2);
    
    float cloudsFormComb = saturate(clouds1+clouds2);
    
    float cloudCol = saturate(saturate(1.0-pow(color1,1.0)*0.2)*bright);
    vec4 clouds1Color = vec4(cloudCol,cloudCol,cloudCol,1.0);
    vec4 clouds2Color = mix(clouds1Color,vec4(1.4, 1.4, 1.4, 0.6),0.4);
    vec4 cloudColComb = mix(clouds1Color,clouds2Color,saturate(clouds2-clouds1));
  output0 = mix(vec4(1.0, 1.0, 1.0, 0.0),cloudColComb,cloudsFormComb);*/
	
	
#if CLOUDS
  const float cloud_scale = 0.03;
  
	vec3 blend = getTriPlanarBlend(v_normal);
	
	float xaxisLayer1 = texture(u_noiseMap, v_position.yz + vec2(u_globalTime*0.2*timeScale)).r;
	float yaxisLayer1 = texture(u_noiseMap, v_position.xz + vec2(u_globalTime*0.2*timeScale)).r;
	float zaxisLayer1 = texture(u_noiseMap, v_position.xy + vec2(u_globalTime*0.2*timeScale)).r;
	vec4 layer1 = vec4(vec3(xaxisLayer1*blend.x + yaxisLayer1*blend.y + zaxisLayer1*blend.z), 1.0);
	layer1.a = clamp(smoothstep(1.0-skyCover,min((1.0-skyCover)+softness*2.0,1.0),layer1.r), 0.0, 1.0);
	layer1.rgba *= 3.0;
  
  
	float xaxisLayer2 = texture(u_noiseMap, -v_position.yz*0.6 - vec2(u_globalTime*0.1*timeScale)).r;
	float yaxisLayer2 = texture(u_noiseMap, -v_position.xz*0.6 - vec2(u_globalTime*0.1*timeScale)).r;
	float zaxisLayer2 = texture(u_noiseMap, -v_position.xy*0.6 - vec2(u_globalTime*0.1*timeScale)).r;
	vec4 layer2 = vec4(vec3(xaxisLayer2*blend.x + yaxisLayer2*blend.y + zaxisLayer2*blend.z), 1.0);
	layer2.a = clamp(smoothstep(1.0-skyCover,min((1.0-skyCover)+softness*2.0,1.0),layer2.r), 0.0, 1.0);
  
	output0 += (layer1+layer2) * v4MieColor;
  
#endif // CLOUDS
	
  output0 = 1.0 - exp(-fExposure * output0);
}
