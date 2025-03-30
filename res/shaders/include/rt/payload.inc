struct RayPayload
{
	vec4 color;
	float distance;
	vec3 normal;
	float roughness;
#if defined(PATHTRACER) || defined(LIGHTMAPPER) || defined(DDGI)
	vec4 throughput;
	vec4 emissive;
	vec4 beta;
#endif
#ifdef LIGHTMAPPER
	vec3 barycentric_coords;
	uint entity_index;
	uint triangle_index;
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