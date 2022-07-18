#ifndef HYP_AABB_GLSL
#define HYP_AABB_GLSL

struct AABB {
    vec3 min;
    vec3 max;
};

vec3 AABBGetCenter(AABB aabb)
{
    return (aabb.max + aabb.min) * 0.5;
}

vec3 AABBGetExtent(AABB aabb)
{
    return aabb.max - aabb.min;
}

float AABBGetGreatestExtent(AABB aabb)
{
    vec3 extent = AABBGetExtent(aabb);

    return max(extent.x, max(extent.y, extent.z));
}

void AABBToSphere(AABB aabb, out vec3 center, out float radius)
{
    //float greatest_extent = AABBGetGreatestExtent(aabb);
    center = AABBGetCenter(aabb);
    radius = length(AABBGetExtent(aabb)) * 0.5;  //greatest_extent * 0.5;
}

#endif