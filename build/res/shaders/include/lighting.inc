struct DirectionalLight 
{
	vec3 direction;
	vec4 color;
};

uniform DirectionalLight env_DirectionalLight;


float sqr(float x)
{
  return x * x;
}

float GGX(float alpha, float cosThetaM)
{
  const float PI = 3.141592654;
  float CosSquared = cosThetaM * cosThetaM;
  float TanSquared = (1.0 - CosSquared) / CosSquared;
  return (1.0 / PI) * sqr(alpha / (CosSquared * (alpha * alpha + TanSquared)));
}

float SchlickFresnel(float f0, float f90, float u)
{
  return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

float CookTorranceG(float NdotL, float NdotV, float LdotH, float NdotH)
{
  return min(1.0, 2.0 * (NdotH / LdotH) * min(NdotL, NdotV));
}

float SpecularDirectional(vec3 n, vec3 v, vec3 lightDir, float roughness)
{
    vec3 vHalfVector = normalize(lightDir + v);

	float NdotH = dot(n, vHalfVector);
	float LdotH = dot(lightDir, vHalfVector);

	float D = GGX(roughness, NdotH);
	float F = SchlickFresnel(0.2, 1.0, LdotH);

	return F * D;
}

vec4 CalculateFogExp(vec4 inColor, vec4 toColor, vec3 worldPos, vec3 camPos, float fogStart, float fogEnd)
{
	float dist = distance(worldPos, camPos);

	float density = 0.003;
	float fogFactor = 1.0 / exp(dist * density);
	vec4 color = mix(inColor, toColor, 1.0-fogFactor);
	color.a = inColor.a;
	return color;
}

vec4 CalculateFogLinear(vec4 inColor, vec4 toColor, vec3 worldPos, vec3 camPos, float fogStart, float fogEnd)
{
	float dist = distance(worldPos, camPos);

	float fogFactor = (fogEnd - dist)/(fogEnd - fogStart);
	fogFactor = clamp(fogFactor, 0.0, 1.0);
	return mix(inColor, toColor, 1.0-fogFactor);
}
