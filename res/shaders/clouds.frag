#version 330 core

in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord0;

uniform float m_GlobalTime;
uniform sampler2D m_CloudMap;
uniform vec4 m_CloudColor;

const float timeScale = 0.3;
const float cloudScale = 0.0005;
const float skyCover = 0.45;
const float softness = 0.4;
const float brightness = 1.3;
const int noiseOctaves = 4;

float saturate(float num)
{
  return clamp(num, 0.0, 1.0);
}

float noise(vec2 uv)
{
  return texture(m_CloudMap, uv).r;
}

vec2 rotate(vec2 uv)
{
  uv = uv + noise(uv*0.2) * 0.005;
  /*float rot = 0.5;
  float sinRot=sin(rot);
  float cosRot=cos(rot);*/
  
  // optimized
  float sinRot = 0.479426;
  float cosRot = 0.877583;
  
  mat2 rotMat = mat2(cosRot, -sinRot, sinRot, cosRot);
  return uv * rotMat;
}

float fbm (vec2 uv)
{
  /*const float rot = 1.57;
  float sinRot=sin(rot);
  float cosRot=cos(rot);*/
  
  // optimized
  float sinRot = 0.999999;
  float cosRot = 0.000796;
  
  float f = 0.0;
  float total = 0.0;
  float mul = 0.5;
  mat2 rotMat = mat2(cosRot, -sinRot, sinRot, cosRot);
    
  for(int i = 0; i < noiseOctaves; i++) {
    f += noise(uv + m_GlobalTime * 0.00015 * timeScale * (1.0 - mul)) * mul;
    total += mul;
    uv *= 3.0;
    uv = rotate(uv);
    mul *= 0.5;
  }
  return f / total;
}

void main() 
{
  float color1 =  fbm(v_texcoord0 - vec2(0.5 + m_GlobalTime * 0.04 * timeScale));
  float color2 = fbm(v_texcoord0 - vec2(10.5 + m_GlobalTime * 0.02 * timeScale));
            
  float clouds1 = smoothstep(1.0 - skyCover, min((1.0 - skyCover) + softness * 2.0, 1.0), color1);
  float clouds2 = smoothstep(1.0 - skyCover, min((1.0 - skyCover) + softness, 1.0), color2);

  float cloudsFormComb = saturate(clouds1 + clouds2);

  float cloudCol = saturate(saturate(1.0 - color1 * 0.2) * brightness);
  vec4 clouds1Color = vec4(cloudCol, cloudCol, cloudCol, 1.0);
  vec4 clouds2Color = mix(clouds1Color, vec4(0.0, 0.0, 0.0, 0.0), 0.25);
  vec4 cloudColComb = mix(clouds1Color, clouds2Color, saturate(clouds2 - clouds1));
  cloudColComb = mix(vec4(1.0, 1.0, 1.0, 0.0), cloudColComb, cloudsFormComb);// * m_CloudColor;
            
  float dist = gl_FragCoord.z / gl_FragCoord.w; 
  vec4 fogColor = vec4(1.0, 1.0, 1.0, 0.0);
  float fogFactor = (30.0 - dist) / (30.0 - 18.0);
  fogFactor = saturate(fogFactor);
  
  gl_FragColor = mix(fogColor, cloudColComb, fogFactor);          
}

