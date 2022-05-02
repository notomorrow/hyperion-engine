struct RayPayload {
	vec3  color;
	float distance;
	vec3  normal;
	float roughness;
};

struct RayProbePayload {
	vec3 color;
	vec3 normal;
	float distance;
	float rand;
	vec3 diffuse;
};