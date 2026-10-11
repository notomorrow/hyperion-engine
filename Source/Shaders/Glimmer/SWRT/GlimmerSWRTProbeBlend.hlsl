#include "../../Include/Defines.hlsli"

PERMUTE(MODE, BLEND, COMPOSE)

#include "GlimmerSWRTCommon.hlsli"

#include "GlimmerProbeTypes.hlsli"
#include "../GlimmerLight.hlsli"

struct GlimmerProbeBlendConstants
{
    GlimmerProbeVolume volume;

    /// x = probes traced per frame at most
    /// y = moves a probe gets to get out of a solid
    /// z = estimates the history averages at least while its light holds
    /// w = same but while it changes
    uint4 dispatch;

    /// x = seconds the history spans while the light holds
    /// y = same but while it changes
    /// z = how far past a back face a probe moves (m)
    /// w = longest sleep interval
    float4 params;

    GlimmerLightParams light;
};

DECLARE_BUFFER_DYNAMIC(GlimmerProbeBlend, CBuffer) cbuffer CBuffer
{
    GlimmerProbeBlendConstants constants;
};

/// 3 per part per probe: what the probe receives of each part of the light
DECLARE_UAV(GlimmerProbeBlend, OutLight) RWStructuredBuffer<float4> OutLight;

/// 3 per probe: the parts put together under the current lighting
DECLARE_UAV(GlimmerProbeBlend, OutSH) RWStructuredBuffer<float4> OutSH;

#define GROUP_SIZE GLIMMER_PROBE_VISIBILITY_TEXELS

void GlimmerWriteComposed(uint probeIndex, float4 light[GLIMMER_LIGHT_PARTS * 3])
{
    float4 composed[3] = { (float4)0.0, (float4)0.0, (float4)0.0 };

    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        const float3 weight = constants.light.weights[part].rgb;

        composed[0] += light[part * 3u + 0u] * weight.r;
        composed[1] += light[part * 3u + 1u] * weight.g;
        composed[2] += light[part * 3u + 2u] * weight.b;
    }

    OutSH[probeIndex * 3u + 0u] = composed[0];
    OutSH[probeIndex * 3u + 1u] = composed[1];
    OutSH[probeIndex * 3u + 2u] = composed[2];
}

#if defined(MODE_COMPOSE)

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint probeIndex = groupId.x * GROUP_SIZE + groupIndex;

    if (probeIndex >= GLIMMER_PROBE_POOL_PROBES)
    {
        return;
    }

    float4 light[GLIMMER_LIGHT_PARTS * 3];

    for (uint entry = 0; entry < GLIMMER_LIGHT_PARTS * 3; entry++)
    {
        light[entry] = OutLight[probeIndex * (GLIMMER_LIGHT_PARTS * 3u) + entry];
    }

    GlimmerWriteComposed(probeIndex, light);
}

#else

DECLARE_SRV(GlimmerProbeBlend, Rays) StructuredBuffer<uint> rays;
DECLARE_SRV(GlimmerProbeBlend, GlimmerProbeUpdateListBuffer) StructuredBuffer<uint> glimmerProbeUpdateList;
DECLARE_SRV(GlimmerProbeBlend, GlimmerProbeCountersBuffer) StructuredBuffer<uint> glimmerProbeCounters;
DECLARE_SRV(GlimmerProbeBlend, GlimmerProbeSlotsBuffer) StructuredBuffer<int4> glimmerProbeSlots;

DECLARE_UAV(GlimmerProbeBlend, OutStates) RWStructuredBuffer<uint4> OutStates;
DECLARE_UAV(GlimmerProbeBlend, OutVisibility) RWStructuredBuffer<uint> OutVisibility;   // GLIMMER_PROBE_VISIBILITY_TEXELS per probe (GlimmerPackHalf2)
DECLARE_UAV(GlimmerProbeBlend, OutBlockWake) RWStructuredBuffer<uint> OutBlockWake;

/// x = variance of an estimate's luminance, relative to the luminance squared
/// y = recent mean of how far estimates fall off the history (in standard deviations)
/// z = recent mean of that distance squared (unscaled), relative like x
/// w = estimates the history averages
DECLARE_UAV(GlimmerProbeBlend, OutTrend) RWStructuredBuffer<float4> OutTrend;

#define GLIMMER_PROBES_NO_SAMPLING
#include "GlimmerProbes.hlsli"
#include "../GlimmerConstants.hlsli"

// what the relative trend values are relative to at least
#define GLIMMER_PROBE_MIN_TREND_LUMINANCE 1e-3

/// hit distance (negative on back faces, 0 starting inside)
groupshared float gsRayDistances[GLIMMER_PROBE_MAX_RAYS];
groupshared float3 gsRayLight[GLIMMER_PROBE_MAX_RAYS * GLIMMER_LIGHT_PARTS];

groupshared float3 gsDirections[GLIMMER_PROBE_MAX_RAYS];
groupshared uint gsWriteVisibility;
groupshared float gsVisibilityBlend;

float GlimmerLuminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 GlimmerClampOffset(float3 offset)
{
    return clamp(offset, -GLIMMER_PROBE_MAX_OFFSET, GLIMMER_PROBE_MAX_OFFSET);
}

float GlimmerComposedLuminance(float4 light[GLIMMER_LIGHT_PARTS * 3])
{
    float3 composed = (float3)0.0;

    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        composed += float3(light[part * 3u + 0u].x, light[part * 3u + 1u].x, light[part * 3u + 2u].x) * constants.light.weights[part].rgb;
    }

    return GlimmerLuminance(composed);
}

void GlimmerBlendProbe(uint probeIndex, uint4 state, uint numRays, float invSpacing)
{
    uint probeState = GlimmerProbeStateOf(state);

    float3 e0[GLIMMER_LIGHT_PARTS];
    float3 e1R[GLIMMER_LIGHT_PARTS];
    float3 e1G[GLIMMER_LIGHT_PARTS];
    float3 e1B[GLIMMER_LIGHT_PARTS];

    [unroll]
    for (uint clearPart = 0; clearPart < GLIMMER_LIGHT_PARTS; clearPart++)
    {
        e0[clearPart] = (float3)0.0;
        e1R[clearPart] = (float3)0.0;
        e1G[clearPart] = (float3)0.0;
        e1B[clearPart] = (float3)0.0;
    }

    float luminanceSquaredSum = 0.0;
    uint numSeen = 0u;

    uint numBackfaces = 0u;
    uint numStartsInside = 0u;

    float nearestBackface = 1e30;
    float3 nearestBackfaceDirection = (float3)0.0;

    float nearestFrontface = 1e30;
    float3 nearestFrontfaceDirection = (float3)0.0;

    float3 freeDirectionSum = (float3)0.0;

    for (uint rayIndex = 0; rayIndex < numRays; rayIndex++)
    {
        const float rayDistance = gsRayDistances[rayIndex];
        const float3 direction = gsDirections[rayIndex];

        if (rayDistance < 0.0)
        {
            numBackfaces++;

            if (-rayDistance < nearestBackface)
            {
                nearestBackface = -rayDistance;
                nearestBackfaceDirection = direction;
            }

            continue;
        }

        if (rayDistance == 0.0)
        {
            numStartsInside++;

            continue;
        }

        float3 composed = (float3)0.0;

        [unroll]
        for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
        {
            const float3 value = gsRayLight[rayIndex * GLIMMER_LIGHT_PARTS + part];

            e0[part] += value;
            e1R[part] += value.r * direction;
            e1G[part] += value.g * direction;
            e1B[part] += value.b * direction;

            composed += value * constants.light.weights[part].rgb;
        }

        luminanceSquaredSum += GlimmerLuminance(composed) * GlimmerLuminance(composed);
        numSeen++;

        freeDirectionSum += direction;

        if (rayDistance < nearestFrontface)
        {
            nearestFrontface = rayDistance;
            nearestFrontfaceDirection = direction;
        }
    }

    uint relocations = GlimmerProbeRelocations(state);
    uint updates = GlimmerProbeUpdates(state);
    float3 offset = GlimmerUnpackProbeOffset(state.y);

    const float time = constants.volume.params.x;

    const float insideFraction = select(
        probeState == GLIMMER_PROBE_STATE_INSIDE,
        GLIMMER_PROBE_OUTSIDE_FRACTION,
        GLIMMER_PROBE_INSIDE_FRACTION);

    // inside a solid: move out and start over, a few times, before giving up on the probe
    if (float(numBackfaces + numStartsInside) >= insideFraction * float(numRays))
    {
        const uint insideStreak = GlimmerProbeInsideStreak(state) + 1u;

        if (probeState == GLIMMER_PROBE_STATE_ACTIVE && updates > 0u && insideStreak < GLIMMER_PROBE_INSIDE_STREAK)
        {
            OutStates[probeIndex] = uint4(GlimmerWithInsideStreak(state.x, insideStreak), state.y, asuint(time), state.w);

            gsWriteVisibility = 0u;

            return;
        }

        if (relocations < constants.dispatch.y)
        {
            if (numBackfaces != 0u)
            {
                // past the nearest back face
                offset += nearestBackfaceDirection * ((nearestBackface + constants.params.z) * invSpacing);
            }
            else if (dot(freeDirectionSum, freeDirectionSum) > 1e-6)
            {
                // toward the rays that got out
                offset += normalize(freeDirectionSum) * GLIMMER_PROBE_FREE_STEP;
            }

            OutStates[probeIndex] = uint4(
                GlimmerPackProbeFlags(GLIMMER_PROBE_STATE_ACTIVE, relocations + 1u, 0u, numBackfaces, numStartsInside),
                GlimmerPackProbeOffset(GlimmerClampOffset(offset)),
                asuint(time),
                state.w);
        }
        else
        {
            OutStates[probeIndex] = uint4(GlimmerPackProbeFlags(GLIMMER_PROBE_STATE_INSIDE, relocations, 0u, numBackfaces, numStartsInside), state.y, asuint(time), state.w);
        }

        gsWriteVisibility = 0u;

        return;
    }

    if (probeState == GLIMMER_PROBE_STATE_INSIDE)
    {
        probeState = GLIMMER_PROBE_STATE_ACTIVE;
        updates = 0u;
        relocations = 0u;
    }

    // right up against a front face, half the probe's view is that face; nudge it off
    if (nearestFrontface * invSpacing < GLIMMER_PROBE_MIN_CLEARANCE)
    {
        offset = GlimmerClampOffset(offset - nearestFrontfaceDirection * (GLIMMER_PROBE_MIN_CLEARANCE - nearestFrontface * invSpacing));
    }
    else if (numBackfaces == 0u && numStartsInside == 0u && nearestFrontface * invSpacing > GLIMMER_PROBE_RETURN_CLEARANCE)
    {
        const float offsetLength = length(offset);

        if (offsetLength > 1e-4)
        {
            offset *= max(offsetLength - GLIMMER_PROBE_RETURN_STEP, 0.0) / offsetLength;
        }
    }

    const float invNumSeen = 1.0 / float(max(numSeen, 1u));

    float4 light[GLIMMER_LIGHT_PARTS * 3];

    [unroll]
    for (uint projectPart = 0; projectPart < GLIMMER_LIGHT_PARTS; projectPart++)
    {
        light[projectPart * 3u + 0u] = float4(e0[projectPart].r * invNumSeen, e1R[projectPart] * (2.0 * invNumSeen));
        light[projectPart * 3u + 1u] = float4(e0[projectPart].g * invNumSeen, e1G[projectPart] * (2.0 * invNumSeen));
        light[projectPart * 3u + 2u] = float4(e0[projectPart].b * invNumSeen, e1B[projectPart] * (2.0 * invNumSeen));
    }

    // the light is judged put together under the lighting of now, so a brighter sun or sky isn't a change of the probe's
    const float estimateLuminance = GlimmerComposedLuminance(light);

    const float estimateVariance = max(luminanceSquaredSum * invNumSeen - estimateLuminance * estimateLuminance, 0.0) * invNumSeen;

    const float updateInterval = max(time - asfloat(state.z), 1e-3);

    const float maxHistory = clamp(constants.params.x / updateInterval, float(max(constants.dispatch.z, 1u)), GLIMMER_PROBE_MAX_HISTORY);
    const float minHistory = min(max(constants.params.y / updateInterval, float(constants.dispatch.w)), maxHistory);

    float4 trend = OutTrend[probeIndex];

    bool idle = false;

    if (updates > 0u)
    {
        float4 previous[GLIMMER_LIGHT_PARTS * 3];

        for (uint loadEntry = 0; loadEntry < GLIMMER_LIGHT_PARTS * 3; loadEntry++)
        {
            previous[loadEntry] = OutLight[probeIndex * (GLIMMER_LIGHT_PARTS * 3u) + loadEntry];
        }

        const float historyLuminance = GlimmerComposedLuminance(previous);

        const float trendScale = max(historyLuminance, GLIMMER_PROBE_MIN_TREND_LUMINANCE);
        const float trendScaleSquared = trendScale * trendScale;

        const float sigma = max(sqrt(trend.x) * trendScale, GLIMMER_PROBE_MIN_RELATIVE_SIGMA * historyLuminance + 1e-5);
        const float difference = estimateLuminance - historyLuminance;

        const float deviation = difference / max(sqrt(trend.z) * trendScale, sigma);

        const float drift = lerp(trend.y, clamp(deviation, -GLIMMER_PROBE_DRIFT_STEP_DOWN, GLIMMER_PROBE_DRIFT_STEP_UP), GLIMMER_PROBE_DRIFT_RATE);

        // a history off by b standard deviations is worth no more than (1 / b)^2 estimates
        const float bias = drift * drift - GLIMMER_PROBE_DRIFT_NOISE;

        float history = min(trend.w, maxHistory);

        if (bias > 0.0)
        {
            history = min(history, max(1.0 / bias, minHistory));
        }

        idle = bias <= 0.0 && history + 1.0 >= maxHistory;

        if (bias > GLIMMER_PROBE_DRIFT_NOISE * GLIMMER_PROBE_WAKE_BLOCK_BIAS)
        {
            OutBlockWake[probeIndex / GLIMMER_PROBES_PER_BLOCK] = constants.volume.info.z;
        }

        float fireflyScale = 1.0;

        if (history >= GLIMMER_PROBE_FIREFLY_MIN_HISTORY && difference > GLIMMER_PROBE_FIREFLY_SIGMAS * sigma)
        {
            fireflyScale = (historyLuminance + GLIMMER_PROBE_FIREFLY_SIGMAS * sigma) / max(estimateLuminance, 1e-6);
        }

        const float blend = 1.0 / (history + 1.0);

        for (uint blendEntry = 0; blendEntry < GLIMMER_LIGHT_PARTS * 3; blendEntry++)
        {
            light[blendEntry] = lerp(previous[blendEntry], light[blendEntry] * fireflyScale, blend);
        }

        const float blendedScale = max(GlimmerComposedLuminance(light), GLIMMER_PROBE_MIN_TREND_LUMINANCE);
        const float invBlendedScaleSquared = 1.0 / (blendedScale * blendedScale);

        trend = float4(
            lerp(trend.x * trendScaleSquared, estimateVariance, GLIMMER_PROBE_NOISE_RATE) * invBlendedScaleSquared,
            drift,
            lerp(trend.z * trendScaleSquared, difference * difference, GLIMMER_PROBE_SPREAD_RATE) * invBlendedScaleSquared,
            min(history + 1.0, maxHistory));
    }
    else
    {
        const float estimateScale = max(estimateLuminance, GLIMMER_PROBE_MIN_TREND_LUMINANCE);
        const float relativeVariance = estimateVariance / (estimateScale * estimateScale);

        trend = float4(relativeVariance, 0.0, 2.0 * relativeVariance, 1.0);
    }

    if (relocations != 0u && updates >= GLIMMER_PROBE_SETTLE_UPDATES)
    {
        relocations = 0u;
    }

    for (uint storeEntry = 0; storeEntry < GLIMMER_LIGHT_PARTS * 3; storeEntry++)
    {
        OutLight[probeIndex * (GLIMMER_LIGHT_PARTS * 3u) + storeEntry] = light[storeEntry];
    }

    GlimmerWriteComposed(probeIndex, light);

    const uint sleepInterval = idle && relocations == 0u
        ? min(GlimmerProbeSleepInterval(state) + 1u, uint(constants.params.w))
        : 0u;

    OutTrend[probeIndex] = trend;
    OutStates[probeIndex] = uint4(
        GlimmerWithSleepInterval(GlimmerPackProbeFlags(probeState, relocations, updates + 1u, numBackfaces, numStartsInside), sleepInterval),
        GlimmerPackProbeOffset(offset),
        asuint(time),
        GlimmerPackGoodOffset(offset));

    // the depths only change with the geometry, which resets updates when it moves the probe, so they don't follow the light's history
    gsWriteVisibility = 1u;
    gsVisibilityBlend = 1.0 / (min(float(updates), maxHistory) + 1.0);
}

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint listIndex = groupId.x;

    if (listIndex >= min(glimmerProbeCounters[0], constants.dispatch.x))
    {
        return;
    }

    const uint probeIndex = glimmerProbeUpdateList[listIndex];
    const int4 slot = glimmerProbeSlots[probeIndex / GLIMMER_PROBES_PER_BLOCK];

    const uint4 state = OutStates[probeIndex];
    const uint probeState = GlimmerProbeStateOf(state);

    if (slot.w < 0 || (probeState != GLIMMER_PROBE_STATE_ACTIVE && probeState != GLIMMER_PROBE_STATE_INSIDE))
    {
        return;
    }

    const float invSpacing = constants.volume.levels[slot.w].params.y;
    const uint numRays = min(constants.volume.info.y, GLIMMER_PROBE_MAX_RAYS);

    if (groupIndex < numRays)
    {
        const uint base = (listIndex * numRays + groupIndex) * GLIMMER_RAY_UINTS;

        gsRayDistances[groupIndex] = asfloat(rays[base]);

        [unroll]
        for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
        {
            gsRayLight[groupIndex * GLIMMER_LIGHT_PARTS + part] = GlimmerUnpackLightPart(uint2(rays[base + 1u + part * 2u], rays[base + 2u + part * 2u]));
        }

        gsDirections[groupIndex] = GlimmerProbeRayDirection(constants.volume, probeIndex, groupIndex);
    }

    GroupMemoryBarrierWithGroupSync();

    if (groupIndex == 0u)
    {
        GlimmerBlendProbe(probeIndex, state, numRays, invSpacing);
    }

    GroupMemoryBarrierWithGroupSync();

    if (gsWriteVisibility == 0u)
    {
        return;
    }

    const uint texel = groupIndex;
    const float2 texelUV = (float2(texel % GLIMMER_PROBE_VISIBILITY_RES, texel / GLIMMER_PROBE_VISIBILITY_RES) + 0.5) / float(GLIMMER_PROBE_VISIBILITY_RES);
    const float3 texelDirection = GlimmerOctahedralDecode(texelUV);

    float3 sums = (float3)0.0; // weighted depth, weighted depth squared, weight

    for (uint rayIndex = 0; rayIndex < numRays; rayIndex++)
    {
        const float weight = pow(saturate(dot(texelDirection, gsDirections[rayIndex])), GLIMMER_PROBE_VISIBILITY_SHARPNESS);
        const float depth = min(abs(gsRayDistances[rayIndex]) * invSpacing, GLIMMER_PROBE_DEPTH_RANGE);

        sums += float3(depth, depth * depth, 1.0) * weight;
    }

    if (sums.z > 1e-4)
    {
        const uint visibilityIndex = probeIndex * GLIMMER_PROBE_VISIBILITY_TEXELS + texel;

        const float2 estimate = sums.xy / sums.z;
        const float texelBlend = gsVisibilityBlend < 1.0 ? gsVisibilityBlend * saturate(sums.z) : 1.0;

        OutVisibility[visibilityIndex] = GlimmerPackHalf2(lerp(GlimmerUnpackHalf2(OutVisibility[visibilityIndex]), estimate, texelBlend));
    }
}

#endif
