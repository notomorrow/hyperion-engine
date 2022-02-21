#version 330 core

#include "include/frag_output.inc"
#include "include/lighting.inc"
#include "include/tonemap.inc"

uniform vec3 v3LightPos;
uniform float fg;
uniform float fg2;
uniform float fExposure;

#define $RAYLEIGH vec4(5.8e-6, 13.5e-6, 33.1e-6, 1.0)
#define $MIE vec4(21e-6, 21e-6, 21e-6, 1.0)

in vec3 v3Direction;
in vec4 v4MieColor;
in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord0;

uniform vec4 u_sunColor;
uniform float u_globalTime;
uniform sampler2D u_noiseMap;


#define $PI 3.141592
#define $iSteps 16
#define $jSteps 8

vec2 rsi(vec3 r0, vec3 rd, float sr) {
    // ray-sphere intersection that assumes
    // the sphere is centered at the origin.
    // No intersection when result.x > result.y
    float a = dot(rd, rd);
    float b = 2.0 * dot(rd, r0);
    float c = dot(r0, r0) - (sr * sr);
    float d = (b*b) - 4.0*a*c;
    if (d < 0.0) return vec2(1e5,-1e5);
    return vec2(
        (-b - sqrt(d))/(2.0*a),
        (-b + sqrt(d))/(2.0*a)
    );
}

vec3 atmosphere(vec3 r, vec3 r0, vec3 pSun, float iSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie, float shRlh, float shMie, float g) {
    // Normalize the sun and view directions.
    pSun = normalize(pSun);
    r = normalize(r);

    // Calculate the step size of the primary ray.
    vec2 p = rsi(r0, r, rAtmos);
    if (p.x > p.y) return vec3(0,0,0);
    p.y = min(p.y, rsi(r0, r, rPlanet).x);
    float iStepSize = (p.y - p.x) / float($iSteps);

    // Initialize the primary ray time.
    float u_globalTime = 0.0;

    // Initialize accumulators for Rayleigh and Mie scattering.
    vec3 totalRlh = vec3(0,0,0);
    vec3 totalMie = vec3(0,0,0);

    // Initialize optical depth accumulators for the primary ray.
    float iOdRlh = 0.0;
    float iOdMie = 0.0;

    // Calculate the Rayleigh and Mie phases.
    float mu = dot(r, pSun);
    float mumu = mu * mu;
    float gg = g * g;
    float pRlh = 3.0 / (16.0 * $PI) * (1.0 + mumu);
    float pMie = 3.0 / (8.0 * $PI) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * g, 1.5) * (2.0 + gg));

    // Sample the primary ray.
    for (int i = 0; i < $iSteps; i++) {

        // Calculate the primary ray sample position.
        vec3 iPos = r0 + r * (u_globalTime + iStepSize * 0.5);

        // Calculate the height of the sample.
        float iHeight = length(iPos) - rPlanet;

        // Calculate the optical depth of the Rayleigh and Mie scattering for this step.
        float odStepRlh = exp(-iHeight / shRlh) * iStepSize;
        float odStepMie = exp(-iHeight / shMie) * iStepSize;

        // Accumulate optical depth.
        iOdRlh += odStepRlh;
        iOdMie += odStepMie;

        // Calculate the step size of the secondary ray.
        float jStepSize = rsi(iPos, pSun, rAtmos).y / float($jSteps);

        // Initialize the secondary ray time.
        float jTime = 0.0;

        // Initialize optical depth accumulators for the secondary ray.
        float jOdRlh = 0.0;
        float jOdMie = 0.0;

        // Sample the secondary ray.
        for (int j = 0; j < $jSteps; j++) {

            // Calculate the secondary ray sample position.
            vec3 jPos = iPos + pSun * (jTime + jStepSize * 0.5);

            // Calculate the height of the sample.
            float jHeight = length(jPos) - rPlanet;

            // Accumulate the optical depth.
            jOdRlh += exp(-jHeight / shRlh) * jStepSize;
            jOdMie += exp(-jHeight / shMie) * jStepSize;

            // Increment the secondary ray time.
            jTime += jStepSize;
        }

        // Calculate attenuation.
        vec3 attn = exp(-(kMie * (iOdMie + jOdMie) + kRlh * (iOdRlh + jOdRlh)));

        // Accumulate scattering.
        totalRlh += odStepRlh * attn;
        totalMie += odStepMie * attn;

        // Increment the primary ray time.
        u_globalTime += iStepSize;

    }

    // Calculate and return the final color.
    return iSun * (pRlh * kRlh * totalRlh + pMie * kMie * totalMie);
}


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
  vec3 color = atmosphere(
        normalize(v_position.xyz),           // normalized ray direction
        vec3(0,6372e3,0),               // ray origin
        env_DirectionalLight.direction,                        // position of the sun
        22.0,                           // intensity of the sun
        6371e3,                         // radius of the planet in meters
        6471e3,                         // radius of the atmosphere in meters
        vec3(5.5e-6, 13.0e-6, 22.4e-6), // Rayleigh scattering coefficient
        21e-6,                          // Mie scattering coefficient
        8e3,                            // Rayleigh scale height
        1.2e3,                          // Mie scale height
        0.758                           // Mie preferred scattering direction
    );



  float fCos = dot(env_DirectionalLight.direction, v3Direction) / length(v3Direction);
  float fCos2 = fCos*fCos;
  vec4 skyColor = $RAYLEIGH;

  color += (getMiePhase(fCos, fCos2, fg, fg2) * v4MieColor * u_sunColor).rgb;//skyColor + getMiePhase(fCos, fCos2, fg, fg2) * $MIE * u_sunColor;

  // todo: standardize in engine
  float aperture = 16.0;
  float shutterSpeed = 1.0/125.0;
  float sensitivity = 100.0;
  float ev100 = log2((aperture * aperture) / shutterSpeed * 100.0f / sensitivity);
  float exposure = 1.0f / (1.2f * pow(2.0f, ev100));


  // Apply exposure.
  //color = 1.0 - exp(-1.0 * color);
  color *= env_DirectionalLight.color.rgb;
  color *= exposure * env_DirectionalLight.intensity;
  color = tonemap(color);
  
  output0 = vec4(color, 1.0);
  output1 = vec4(v_normal * 0.5 + 0.5, 1.0);
  output2 = vec4(v_position.xyz, 1.0);
  output3 = vec4(0.0, 0.0, 0.0, 0.0);
  output4 = vec4(0.0);
}
