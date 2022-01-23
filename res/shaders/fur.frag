#version 330 core
#include "include/lighting.inc"
#include "include/frag_output.inc"
// The higher the value, the bigger the contrast between the fur length.
#define $FUR_STRENGTH_CONTRAST 1.0

// The higher the value, the less fur.
#define $FUR_STRENGTH_CAP 0.1
#define $FUR_ALPHA_THRESHOLD 0.25
#define $FUR_SCALE 2

uniform sampler2D FurStrengthMap;
uniform int HasFurStrengthMap;

in vec3 v_normal;
in vec2 v_texcoord0;
in vec4 v_position;

in float v_furStrength;

void main()
{
    float roughness = clamp(u_roughness, 0.05, 0.99);
    float metallic = clamp(u_shininess, 0.05, 0.99);

	// Note: All calculations are in camera space.

	vec3 normal = normalize(v_normal);

	//intensity += ambientLight.color;
	
	float furStrength = 1.0;
    if (HasFurStrengthMap == 1) {
        furStrength = clamp(v_furStrength * texture(FurStrengthMap, v_texcoord0 * vec2($FUR_SCALE)).r * $FUR_STRENGTH_CONTRAST - $FUR_STRENGTH_CAP, 0.0, 1.0);
    } else {
        furStrength = clamp(v_furStrength * $FUR_STRENGTH_CONTRAST - $FUR_STRENGTH_CAP, 0.0, 1.0);
    }
	//gl_FragColor.rgb = vec3(1.0, 0.0, 0.0);
	if (HasDiffuseMap == 1) {
		output0 = u_diffuseColor * texture(DiffuseMap, v_texcoord0);
	} else {
		output0 = u_diffuseColor;
	}

#if DEFERRED
    output0.a = furStrength;
#endif

#if !DEFERRED
	// Orthogonal fur to light is still illumintated. So shift by one, that only fur targeting away from the light do get darkened. 
	vec4 intensity = vec4(clamp(dot(normal, env_DirectionalLight.direction) , 0.0, furStrength));

    output0 = output0 * intensity;
#endif

    if (furStrength < $FUR_ALPHA_THRESHOLD) {
        discard;
    }

    output1 = vec4(normal * 0.5 + 0.5, 1.0);
    output2 = vec4(v_position.xyz, 1.0);
    output3 = vec4(metallic, roughness, 0.0, 1.0);
    output4 = vec4(0.0, 0.0, 0.0, 1.0 - furStrength);
}   
