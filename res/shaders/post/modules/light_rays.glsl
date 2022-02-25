

#define $VOLUMETRIC_LIGHTING_STEPS 64
#define $VOLUMETRIC_LIGHTING_MIE_G -0.995
#define $VOLUMETRIC_LIGHTING_FADE_START 0.95
#define $VOLUMETRIC_LIGHTING_FADE_END 0.9
#define $VOLUMETRIC_LIGHTING_INTENSITY 0.9

struct VolumetricLight {
    float scattering;
    float extinction;
    float range;
};

float LightRays_Density(vec3 position)
{
    // TODO: noise
    return 1.0;
}

float LightRays_Attenuation(vec3 position, float NdotL)
{
    vec3 shadowCoord = getShadowCoord(0, position);
    float shadow = getShadow(0, shadowCoord, NdotL);
    
    return shadow;
}

float LightRays_MieScattering(float fCos, float fCos2, float g, float g2)
{
    return 1.5 * ((1.0 - g2) / (2.0 + g2)) * (1.0 + fCos2) / pow(1.0 + g2 - 2.0*g*fCos, 1.5);
}

vec4 LightRays_Raymarch(vec2 uv, vec3 rayStart, vec3 rayDir, float rayLength)
{
    VolumetricLight volumetricLight;
    volumetricLight.scattering = 1.0;
    volumetricLight.extinction = 1.0;
    
    float stepSize = rayLength / $VOLUMETRIC_LIGHTING_STEPS;
    vec3 rayDeltaStep = rayDir * stepSize;

    vec3 currentPosition = rayStart + rayDeltaStep;
    
    float volumetric = 0.0;
    
    float extinction = 0.0; // for directional light
    float cosAngle = dot(env_DirectionalLight.direction, -rayDir);
    
    for (int i = 0; i < $VOLUMETRIC_LIGHTING_STEPS; i++) {
        float atten = LightRays_Attenuation(currentPosition, cosAngle);
        //float density = LightRays_Density(currentPosition);
        //float scattering = volumetricLight.scattering * stepSize * density;
        //extinction += volumetricLight.extinction * stepSize * density;
        
        float light = atten;//atten * scattering * density * exp(-extinction);
        
        volumetric += light;
        
        currentPosition += rayDeltaStep;
    }
    
    volumetric /= float($VOLUMETRIC_LIGHTING_STEPS);

    volumetric *= clamp(LightRays_MieScattering(cosAngle, cosAngle*cosAngle, $VOLUMETRIC_LIGHTING_MIE_G, $VOLUMETRIC_LIGHTING_MIE_G * $VOLUMETRIC_LIGHTING_MIE_G), 0.0, 1.0);
    volumetric = max(0.0, volumetric);
    //volumetric *= exp(-extinction);
    
    return env_DirectionalLight.color * volumetric * $VOLUMETRIC_LIGHTING_INTENSITY;
}


void PostProcess_LightRays(PostProcessData data, out PostProcessResult result)
{
    vec3 rayDir = data.worldPosition - data.cameraPosition;
    float rayLength = length(rayDir);
    rayDir /= rayLength;
    rayLength = min(rayLength, data.linearDepth);

	result.color = LightRays_Raymarch(data.texCoord, data.cameraPosition, rayDir, rayLength) * data.exposure * env_DirectionalLight.intensity;
    result.mode = $POST_PROCESS_ADD;
}