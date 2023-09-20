struct RayPayload
{
	vec3 color;
	float distance;
	vec3 normal;
	float roughness;
#ifdef PATHTRACER
    vec3 beta;
	vec3 absorption;
	vec3 dir;
#endif
};

struct RayProbePayload
{
	vec3 color;
	vec3 normal;
	float distance;
	float rand;
	vec3 diffuse;
};

struct RayHit
{
	RayPayload payload;
	vec3 position;
};