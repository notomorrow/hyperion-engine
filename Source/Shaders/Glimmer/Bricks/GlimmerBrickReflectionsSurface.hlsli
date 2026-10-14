#ifndef GLIMMER_BRICK_REFLECTIONS_SURFACE_HLSLI
#define GLIMMER_BRICK_REFLECTIONS_SURFACE_HLSLI

// the surface a pixel reflects from; the includer has camera, the gbuffer, and constants with dimensions, params and bricks as GlimmerBrickReflections.hlsl has them

#define HORIZON 0.03
#define MATTE_BAND 0.15

struct ReflectionSurface
{
    float3 P;
    float3 N;        // as shaded
    float roughness; // perceptual
    float alpha;
    bool isValid;
};

float3 LoadPosition(int2 pixel, out float outDepth)
{
    const int2 clamped = clamp(pixel, int2(0, 0), int2(constants.dimensions.xy) - 1);

    outDepth = GBufferDepthTexture.Load(int3(clamped, 0)).r;

    // texcoord y runs opposite to NDC y
    const float2 texcoord = (float2(pixel) + 0.5) / float2(constants.dimensions.xy);
    const float2 unjitteredTexcoord = texcoord - float2(camera.jitter.x, -camera.jitter.y) * 0.5;

    float4 positionWS = mul(camera.invViewMat, ReconstructViewSpacePositionFromDepth(camera.invProjMat, unjitteredTexcoord, outDepth));
    positionWS /= positionWS.w;

    return positionWS.xyz;
}

ReflectionSurface LoadSurface(int2 pixel)
{
    ReflectionSurface surface;

    float depth;
    surface.P = LoadPosition(pixel, depth);

    const float4 normalSample = GBufferNormalsTexture.Load(int3(pixel, 0));
    const uint materialBits = GBufferMaterialTexture.Load(int3(pixel, 0));

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialBits >> 28u, materialParams);

    surface.N = normalize(GBufferUnpackNormal(normalSample));
    surface.alpha = clamp(materialParams.roughness, 0.001, 0.999);
    surface.roughness = sqrt(surface.alpha);
    surface.isValid = depth < 1.0 && (materialParams.mask & OBJECT_MASK_UNLIT) == 0u;

    return surface;
}

// the way the surface itself faces, which a normal map does not turn: from the depths beside the pixel, the nearer of the two each way
float3 LoadSurfaceNormal(int2 pixel, float3 P, float3 fallback)
{
    float depth;

    const float3 toLeft = LoadPosition(pixel - int2(1, 0), depth) - P;
    const float3 toRight = LoadPosition(pixel + int2(1, 0), depth) - P;
    const float3 toUp = LoadPosition(pixel - int2(0, 1), depth) - P;
    const float3 toDown = LoadPosition(pixel + int2(0, 1), depth) - P;

    const float3 across = dot(toLeft, toLeft) < dot(toRight, toRight) ? -toLeft : toRight;
    const float3 along = dot(toUp, toUp) < dot(toDown, toDown) ? -toUp : toDown;

    const float3 normal = cross(across, along);

    if (dot(normal, normal) < 1e-16)
    {
        return fallback;
    }

    const float3 unit = normalize(normal);

    return dot(unit, camera.position.xyz - P) < 0.0 ? -unit : unit;
}

float MatteShare(float roughness)
{
    const float share = saturate((roughness - constants.params.w) / MATTE_BAND);

    return share > 0.99 ? 1.0 : share;
}

float3 ViewOf(ReflectionSurface surface)
{
    return normalize(camera.position.xyz - surface.P);
}

// the mirror's way, kept off the surface where a normal map would send it in
float3 Reflected(ReflectionSurface surface, float3 surfaceNormal)
{
    const float3 direction = reflect(-ViewOf(surface), surface.N);

    return normalize(direction + surfaceNormal * max(HORIZON - dot(direction, surfaceNormal), 0.0));
}

float3 RayStart(float3 position, float3 normal)
{
    return position + normal * (constants.params.x * constants.bricks.params.z);
}

#endif
