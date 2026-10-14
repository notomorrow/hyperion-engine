#include "../../Include/Defines.hlsli"

// a pixel finds the crossing it is on, which owns a block of four slots, each the hit of one ray in a bin of directions, and reads the light of what the slots name
// REQUESTS traces what blocks lacked last frame, RESOLVE reads slots, FINISH asks and lists, RAYS traces for the listed, HISTORY adds frames up, SPREAD fills every pixel,
// then WHOLE_LIST and WHOLE_RAYS give every pixel of a polished thing a ray: a mirror's picture cannot be spread from one pixel in four
PERMUTE(MODE, REQUESTS, RESOLVE, FINISH, RAYS, HISTORY, SPREAD, WHOLE_LIST, WHOLE_RAYS)

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(GlimmerBrickReflections, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(GlimmerBrickReflections, GBufferDepthTexture) Texture2D GBufferDepthTexture;
DECLARE_SRV(GlimmerBrickReflections, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;

#include "../../Include/Gbuffer.hlsli"
#include "../../Include/Scene.hlsli"

DECLARE_SRV_DYNAMIC(GlimmerBrickReflections, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(GlimmerBrickReflections, GlimmerBrickTableBuffer) StructuredBuffer<uint> glimmerBrickTable;
DECLARE_SRV(GlimmerBrickReflections, GlimmerBrickIndexBuffer) StructuredBuffer<uint> glimmerBrickIndex;
DECLARE_SRV(GlimmerBrickReflections, GlimmerBrickCrossingsBuffer) StructuredBuffer<uint> glimmerBrickCrossings;
DECLARE_SRV(GlimmerBrickReflections, GlimmerBrickMaterialBuffer) StructuredBuffer<uint> glimmerBrickMaterial;
DECLARE_SRV(GlimmerBrickReflections, GlimmerBrickRadianceBuffer) StructuredBuffer<uint> glimmerBrickRadiance;
DECLARE_SRV(GlimmerBrickReflections, GlimmerBrickCrossingLightBuffer) StructuredBuffer<uint> glimmerBrickCrossingLight;

#define GLIMMER_BRICKS_WITH_LIGHT
#include "GlimmerBricks.hlsli"

struct GlimmerBrickReflectionsConstants
{
    GlimmerBrickParams bricks;
    uint4 dimensions; // xy = the view's, zw = the half resolution picture's
    float4 params;    // x = columns off its surface a ray starts, y = alphas a bin is wide, z = how far a ray looks, w = roughness from which the reflection is handed back
    float4 aim;       // x = alphas across a card is read over along a drawn direction, y = how many times what it has added up a sample may be, z = columns a moving dither reaches
    uint4 frame;      // x = frame, y = 1 when pixels read slots, z = frames a pixel that reads slots adds up, w = frames one that traces for itself adds up
    uint4 limits;     // x = crossings, y = blocks, z = requests a frame at most, w = 1 when there is no last frame to read
    uint4 debug;      // x = 1 colours each pixel by how it came by its reflection, y = 1 counts into slotCounts
};

DECLARE_BUFFER_DYNAMIC(GlimmerBrickReflections, CBuffer) cbuffer CBuffer
{
    GlimmerBrickReflectionsConstants constants;
};

// the block of each crossing, where it has one: checked against the block's own word for who it is of
DECLARE_UAV(GlimmerBrickReflections, SlotBlockOfBuffer) RWStructuredBuffer<uint> slotBlockOf;
// ten words a block: its crossing, a hash of that crossing, then four slots of two: what the ray hit, and its key with the ray's length as a half above it
DECLARE_UAV(GlimmerBrickReflections, SlotBlocksBuffer) RWStructuredBuffer<uint> slotBlocks;
DECLARE_UAV(GlimmerBrickReflections, SlotClaimsBuffer) RWStructuredBuffer<uint> slotClaims;
// four words a request: the crossing, which bins, its brick and its column
DECLARE_UAV(GlimmerBrickReflections, SlotRequestsBuffer) RWStructuredBuffer<uint> slotRequests;
// what each resolved pixel found missing, four words as a request has them
DECLARE_UAV(GlimmerBrickReflections, SlotWantedBuffer) RWStructuredBuffer<uint> slotWanted;
DECLARE_UAV(GlimmerBrickReflections, SlotListBuffer) RWStructuredBuffer<uint> slotList;
// the pixels of the view that get a ray after the spread
DECLARE_UAV(GlimmerBrickReflections, SlotWholeBuffer) RWStructuredBuffer<uint> slotWhole;
DECLARE_UAV(GlimmerBrickReflections, SlotCountsBuffer) RWStructuredBuffer<uint> slotCounts;

// rgb = radiance times a, a = how much of the pixel's reflection is the bricks'
DECLARE_UAV(GlimmerBrickReflections, OutColor) RWTexture2D<float4> OutColor;
// x = how far the pixel's rays went, less than 0 where they left the bricks, REACH_HOLE for a pixel with no ray this frame; y = frames added up
DECLARE_UAV(GlimmerBrickReflections, OutInfo) RWTexture2D<float4> OutInfo;
// xyz = position, w = roughness with GUIDE_SAMPLED or GUIDE_DITHERED added for how its light was come by, less than 0 where there is no surface
DECLARE_UAV(GlimmerBrickReflections, OutGuide) RWTexture2D<float4> OutGuide;

DECLARE_SRV(GlimmerBrickReflections, ColorTexture) Texture2D ColorTexture;
DECLARE_SRV(GlimmerBrickReflections, InfoTexture) Texture2D InfoTexture;
DECLARE_SRV(GlimmerBrickReflections, GuideTexture) Texture2D GuideTexture;
DECLARE_SRV(GlimmerBrickReflections, HistoryColorTexture) Texture2D HistoryColorTexture;
DECLARE_SRV(GlimmerBrickReflections, HistoryInfoTexture) Texture2D HistoryInfoTexture;
DECLARE_SRV(GlimmerBrickReflections, HistoryGuideTexture) Texture2D HistoryGuideTexture;

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define COUNT_REQUESTS 0
#define COUNT_LISTED 1
#define COUNT_BLOCK_CURSOR 2
#define COUNT_WHOLE 3
#define COUNT_SLOT_PIXELS 4
#define COUNT_PARTIAL_PIXELS 5
#define COUNT_BLANK_PIXELS 6
#define COUNT_RAY_PIXELS 7

#define BLOCK_WORDS 10u
#define REQUEST_WORDS 4u
#define TRACE_ROW 256u

#define SLOT_SKY 0xFFFFFFFFu
#define SLOT_BELOW 0xFFFFFFFEu

#define WANT_NOTHING 0xFFFFFFFFu
#define WANT_RAY 0xFFFFFFFEu
#define WANT_FILL 0xFFFFFFFDu
#define WANT_BLANK (1u << 27u)

#define LIST_SINGLE 0x80000000u

#define REACH_HOLE -2.0
#define GUIDE_SAMPLED 2.0
#define GUIDE_DITHERED 4.0

#define HORIZON 0.03
// the octahedral grid's bins are this many radians wide at level 0
#define BIN_ANGLE 3.5449
#define MIN_LEVEL 2
#define MAX_LEVEL 6
// the level from which a bin's rays go along the middle of it and not from somewhere in it of each crossing's own
#define SCATTER_BELOW_LEVEL 6
// bins beyond its crossing's own that a pixel's direction may fall and still be read from them
#define STRAY 0.5
// the cosine of how far a pixel's surface may face from its crossing and be flat
#define CURVED_FACING 0.9994
// the roughness from which a pixel that traces for itself places its ray by chance
#define SAMPLED_FROM 0.2
#define SPREAD_DISC 2.0
#define MATTE_BAND 0.15
// the least of a pixel's view worth a read, and the least of it the cards its slots name have to cover for those to be its picture
#define LAYER_LEAST 0.02
#define LAYER_ENOUGH 0.5

#define SAME_SURFACE 0.01
#define VIRTUAL_BELOW 0.3
#define SURFACE_ABOVE 0.6
#define MISS_DISTANCE 100.0
#define ROUGHNESS_TOLERANCE 0.05
#define SLIDE_FORGETS 1.0

#define SPREAD_PLANE_TOLERANCE 0.01

struct ReflectionSurface
{
    float3 P;
    float3 N;        // as shaded
    float roughness; // perceptual
    float alpha;
    bool isValid;
};

uint PcgHash(uint value)
{
    const uint state = value * 747796405u + 2891336453u;
    const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;

    return (word >> 22u) ^ word;
}

uint PixelSeed(uint2 texel)
{
    return PcgHash(texel.x + PcgHash(texel.y));
}

float UnitFloat(uint value)
{
    return float(value >> 8u) * (1.0 / 16777216.0);
}

float InterleavedGradientNoise(float2 position)
{
    return frac(52.9829189 * frac(dot(position, float2(0.06711056, 0.00583715))));
}

void MakeBasis(float3 N, out float3 T, out float3 B)
{
    const float s = N.z >= 0.0 ? 1.0 : -1.0;
    const float a = -1.0 / (s + N.z);
    const float b = N.x * N.y * a;

    T = float3(1.0 + s * N.x * N.x * a, s * b, -s * N.x);
    B = float3(b, s + N.y * N.y * a, -N.y);
}

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

// a direction on the octahedral grid, 0 to 1 each way, with straight up in the middle
float2 DirectionPlace(float3 direction)
{
    const float3 n = direction.xzy / (abs(direction.x) + abs(direction.y) + abs(direction.z));

    float2 e = n.xy;

    if (n.z < 0.0)
    {
        e = (1.0 - abs(n.yx)) * select(n.xy >= 0.0, 1.0, -1.0);
    }

    return e * 0.5 + 0.5;
}

float3 PlaceDirection(float2 place)
{
    const float2 e = place * 2.0 - 1.0;

    float3 n = float3(e, 1.0 - abs(e.x) - abs(e.y));

    const float t = max(-n.z, 0.0);
    n.xy += select(n.xy >= 0.0, -t, t);

    return normalize(n).xzy;
}

uint LevelOf(float alpha)
{
    return uint(clamp(round(log2(BIN_ANGLE / max(constants.params.y * alpha, 1e-4))), float(MIN_LEVEL), float(MAX_LEVEL)));
}

// a bin's place among the bins of every level up to its own, counting from 1
uint BinKey(uint level, int2 bin)
{
    const uint bins = 1u << level;

    return 1u + (bins * bins - 1u) / 3u + uint(bin.y) * bins + uint(bin.x);
}

// the way a bin's ray goes from a crossing: somewhere in the bin of the crossing's own, the same whenever it is asked
float3 BinDirection(uint level, int2 bin, uint crossing)
{
    float2 within = float2(0.5, 0.5);

    if (level < uint(SCATTER_BELOW_LEVEL))
    {
        const uint own = PcgHash(crossing ^ PcgHash(BinKey(level, bin)));

        within = float2(float(own & 0xFFFFu), float(own >> 16u)) / 65536.0;
    }

    return PlaceDirection((float2(bin) + within) / float(1u << level));
}

uint CrossingHash(uint crossing)
{
    return PcgHash(glimmerBrickCrossings[2u * crossing] ^ PcgHash(glimmerBrickCrossings[2u * crossing + 1u] ^ crossing)) | 1u;
}

bool LoadBrickPlace(uint brick, out int3 outCell, out uint outAxis)
{
    const uint header = brick * uint(GLIMMER_BRICK_INDEX_STRIDE);

    outAxis = glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_AXIS)];
    outCell = int3(
        int(glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CELL)]),
        int(glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CELL) + 1u]),
        int(glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CELL) + 2u]));

    return outAxis <= 2u;
}

// what a card sends on around a point over a patch footprint metres across: rgb = radiance times how much of the patch is surface, a = that share
float4 CardLight(int3 cell, uint axis, uint cardSlot, float3 position, float footprint)
{
    const float width = constants.bricks.params.z;

    if (footprint >= float(1u << uint(GLIMMER_BRICK_CARD_FIRST_MIP)) * width * 0.75)
    {
        const float4 light = GlimmerSampleBrickLight(constants.bricks, cell, axis, cardSlot, position, footprint);

        return light.a < 1e-3 ? (float4)0.0 : light;
    }

    // finer than a tile: the crossing of the card in the column under the point, if there is one
    const float3 cellLow = GlimmerBrickCellLow(constants.bricks, cell);
    const float2 across = GlimmerBrickAcross(axis, position - cellLow) * constants.bricks.params.w;
    const int2 beside = int2(floor(across / float(GLIMMER_BRICK_SIDE)));
    const int3 otherCell = cell + int3(GlimmerBrickPoint(axis, float2(beside), 0.0));

    if (!GlimmerBrickCellInWindow(constants.bricks, otherCell))
    {
        return (float4)0.0;
    }

    const uint brick = glimmerBrickTable[GlimmerBrickTableIndex(otherCell, axis)];

    if (brick == GLIMMER_BRICK_NONE)
    {
        return (float4)0.0;
    }

    const int2 column = clamp(int2(floor(across)) - beside * GLIMMER_BRICK_SIDE, int2(0, 0), int2(GLIMMER_BRICK_SIDE - 1, GLIMMER_BRICK_SIDE - 1));

    const uint entry = glimmerBrickIndex[GlimmerBrickColumnIndex(brick, column)];
    const uint first = entry & 0xFFFFFFu;
    const uint count = min(entry >> 24u, uint(GLIMMER_BRICK_MAX_COLUMN_CROSSINGS));

    const float depthAt = GlimmerBrickAlong(axis, position - cellLow);

    uint nearest = GLIMMER_BRICK_NONE;
    float nearestApart = 4.0 * width;

    for (uint k = 0; k < count; k++)
    {
        const uint words = glimmerBrickCrossings[2u * (first + k)];
        const float depth = GlimmerBrickUnpackDepth(constants.bricks, words & 0xFFFFu);

        if (GlimmerBrickCardSlot(constants.bricks, axis, GlimmerBrickUnpackNormal(words >> 16u), depth) == cardSlot && abs(depth - depthAt) < nearestApart)
        {
            nearestApart = abs(depth - depthAt);
            nearest = first + k;
        }
    }

    if (nearest == GLIMMER_BRICK_NONE)
    {
        return (float4)0.0;
    }

    const float4 light = GlimmerBrickUnpackLight(glimmerBrickCrossingLight[nearest]);

    return light.a > 0.0 ? float4(light.rgb, 1.0) : (float4)0.0;
}

float4 HitLight(GlimmerBrickHit hit, float3 position, float footprint)
{
    int3 cell = int3(0, 0, 0);
    uint axis = 0u;

    if (!LoadBrickPlace(hit.brick, cell, axis))
    {
        return (float4)0.0;
    }

    return CardLight(cell, axis, GlimmerBrickCardSlot(constants.bricks, axis, hit.normal, hit.depth), position, footprint);
}

// the half vector of a view's visible normals (Heitz 2018), in the frame of the normal
float3 SampleVisibleNormal(float3 view, float alpha, float2 u)
{
    const float3 stretched = normalize(float3(alpha * view.x, alpha * view.y, view.z));
    const float lengthSquared = stretched.x * stretched.x + stretched.y * stretched.y;

    const float3 t1 = lengthSquared > 0.0 ? float3(-stretched.y, stretched.x, 0.0) * rsqrt(lengthSquared) : float3(1.0, 0.0, 0.0);
    const float3 t2 = cross(stretched, t1);

    const float r = sqrt(u.x);
    const float phi = 2.0 * HYP_FMATH_PI * u.y;
    const float p1 = r * cos(phi);
    const float s = 0.5 * (1.0 + stretched.z);
    const float p2 = (1.0 - s) * sqrt(1.0 - p1 * p1) + s * r * sin(phi);

    const float3 h = p1 * t1 + p2 * t2 + sqrt(max(0.0, 1.0 - p1 * p1 - p2 * p2)) * stretched;

    return normalize(float3(alpha * h.x, alpha * h.y, max(0.0, h.z)));
}

// the way a pixel that reads slots looks this frame: drawn from its lobe, from a start of its own and a step a frame
// the same whoever asks in one frame, so that a ray traced for the pixel later goes the way its cards were read
bool LobeAim(uint2 texel, ReflectionSurface surface, float3 surfaceNormal, out float3 outAim)
{
    outAim = (float3)0.0;

    if (constants.frame.z == 0u)
    {
        return false;
    }

    const uint own = PixelSeed(texel);
    const uint2 stepped = uint2(PcgHash(own ^ 0x51ED270Bu), PcgHash(own ^ 0x2545F491u)) + constants.frame.x * uint2(2700274805u, 2313257605u);

    float3 T;
    float3 B;
    MakeBasis(surface.N, T, B);

    const float3 V = ViewOf(surface);
    const float3 localView = float3(dot(V, T), dot(V, B), dot(V, surface.N));

    if (localView.z <= 0.0)
    {
        return false;
    }

    const float3 h = SampleVisibleNormal(localView, surface.alpha, float2(UnitFloat(stepped.x), UnitFloat(stepped.y)));
    const float3 localDirection = reflect(-localView, h);

    if (localDirection.z <= 0.0)
    {
        return false;
    }

    const float3 direction = T * localDirection.x + B * localDirection.y + surface.N * localDirection.z;

    outAim = normalize(direction + surfaceNormal * max(HORIZON - dot(direction, surfaceNormal), 0.0));

    return true;
}

// up to a square root of two either way, another each frame: a pixel then reads each of the two mips its width lies between
void Note(uint counter)
{
    if (constants.debug.y != 0u)
    {
        uint previous;
        InterlockedAdd(slotCounts[counter], 1u, previous);
    }
}

float BetweenMips(uint own)
{
    return exp2(UnitFloat(own + constants.frame.x * 2654435769u) - 0.5);
}

#if defined(MODE_REQUESTS)

void TraceRequest(uint index)
{
    if (index >= min(slotCounts[COUNT_REQUESTS], constants.limits.z))
    {
        return;
    }

    const uint crossingIndex = slotRequests[REQUEST_WORDS * index];
    const uint code = slotRequests[REQUEST_WORDS * index + 1u];
    const uint brick = slotRequests[REQUEST_WORDS * index + 2u];
    const uint columnIndex = slotRequests[REQUEST_WORDS * index + 3u];

    int3 cell = int3(0, 0, 0);
    uint axis = 0u;

    if (crossingIndex >= constants.limits.x || !LoadBrickPlace(brick, cell, axis))
    {
        return;
    }

    const uint level = code >> 28u;
    const int2 base = int2(int(code & 0x1FFFu), int((code >> 13u) & 0x1FFFu));

    const int2 column = int2(columnIndex % uint(GLIMMER_BRICK_SIDE), columnIndex / uint(GLIMMER_BRICK_SIDE));
    const GlimmerBrickCrossing crossing = GlimmerLoadBrickCrossing(constants.bricks, crossingIndex);

    const float width = constants.bricks.params.z;
    const float2 offset = (GlimmerBrickMaskMiddle(crossing.mask) - 0.5) * width;
    const float2 slope = GlimmerBrickAcross(axis, crossing.normal) / GlimmerBrickAlong(axis, crossing.normal);

    const float3 position = GlimmerBrickCellLow(constants.bricks, cell)
        + GlimmerBrickPoint(axis, (float2(column) + 0.5) * width + offset, crossing.depth - dot(slope, offset));

    const float3 origin = RayStart(position, crossing.normal);

    const uint hash = CrossingHash(crossingIndex);

    uint block = slotBlockOf[crossingIndex];

    const bool hasBlock = block < constants.limits.y && slotBlocks[block * BLOCK_WORDS] == crossingIndex && slotBlocks[block * BLOCK_WORDS + 1u] == hash;

    uint oldCard[4] = { 0u, 0u, 0u, 0u };
    uint oldKey[4] = { 0u, 0u, 0u, 0u };

    if (hasBlock)
    {
        for (uint slot = 0; slot < 4u; slot++)
        {
            oldCard[slot] = slotBlocks[block * BLOCK_WORDS + 2u + 2u * slot];
            oldKey[slot] = slotBlocks[block * BLOCK_WORDS + 3u + 2u * slot];
        }
    }
    else
    {
        // the oldest block is whoever's turn it is
        uint cursor;
        InterlockedAdd(slotCounts[COUNT_BLOCK_CURSOR], 1u, cursor);

        block = cursor % constants.limits.y;
    }

    uint newCard[4] = { 0u, 0u, 0u, 0u };
    uint newKey[4] = { 0u, 0u, 0u, 0u };
    uint wanted[4] = { 0u, 0u, 0u, 0u };
    uint filled = 0u;

    for (uint tap = 0; tap < 4u; tap++)
    {
        const int2 bin = base + int2(tap & 1u, tap >> 1u);
        const uint key = BinKey(level, bin);

        wanted[tap] = key;

        uint held = 4u;

        for (uint heldSlot = 0; heldSlot < 4u; heldSlot++)
        {
            if ((oldKey[heldSlot] & 0xFFFFu) == key)
            {
                held = heldSlot;
            }
        }

        if (held < 4u)
        {
            newCard[filled] = oldCard[held];
            newKey[filled] = oldKey[held];
            filled++;

            continue;
        }

        const float3 direction = BinDirection(level, bin, crossingIndex);

        uint card = SLOT_BELOW;
        float t = 0.0;

        if (dot(direction, crossing.normal) >= HORIZON)
        {
            GlimmerBrickHit hit;

            if (!TraceGlimmerBricks(constants.bricks, origin, direction, 0.0, constants.params.z, hit))
            {
                card = SLOT_SKY;
            }
            else if (dot(hit.normal, direction) < 0.0)
            {
                // the back of something is what a ray sees from inside it: nothing to show
                card = hit.brick * uint(GLIMMER_BRICK_CARDS) + GlimmerBrickCardSlot(constants.bricks, hit.axis, hit.normal, hit.depth);
                t = hit.t;
            }
        }

        newCard[filled] = card;
        newKey[filled] = key | (f32tof16(min(t, 60000.0)) << 16u);
        filled++;
    }

    // behind them, as many of what it held before as there is room for: the oldest go first
    for (uint oldSlot = 0; oldSlot < 4u && filled < 4u; oldSlot++)
    {
        const uint oldSlotKey = oldKey[oldSlot] & 0xFFFFu;

        if (oldSlotKey != 0u && oldSlotKey != wanted[0] && oldSlotKey != wanted[1] && oldSlotKey != wanted[2] && oldSlotKey != wanted[3])
        {
            newCard[filled] = oldCard[oldSlot];
            newKey[filled] = oldKey[oldSlot];
            filled++;
        }
    }

    slotBlocks[block * BLOCK_WORDS] = crossingIndex;
    slotBlocks[block * BLOCK_WORDS + 1u] = hash;

    for (uint writeSlot = 0; writeSlot < 4u; writeSlot++)
    {
        slotBlocks[block * BLOCK_WORDS + 2u + 2u * writeSlot] = newCard[writeSlot];
        slotBlocks[block * BLOCK_WORDS + 3u + 2u * writeSlot] = newKey[writeSlot];
    }

    slotBlockOf[crossingIndex] = block;
}

#endif

#if defined(MODE_RESOLVE)

void SetWant(uint index, uint want, uint code, uint brick, uint column)
{
    slotWanted[REQUEST_WORDS * index] = want;
    slotWanted[REQUEST_WORDS * index + 1u] = code;
    slotWanted[REQUEST_WORDS * index + 2u] = brick;
    slotWanted[REQUEST_WORDS * index + 3u] = column;
}

void Resolve(int2 pixel, uint2 texel, uint index)
{
    SetWant(index, WANT_NOTHING, 0u, 0u, 0u);

    OutColor[texel] = (float4)0.0;
    OutInfo[texel] = (float4)0.0;

    const ReflectionSurface surface = LoadSurface(pixel);

    if (!surface.isValid)
    {
        OutGuide[texel] = float4(0.0, 0.0, 0.0, -1.0);

        return;
    }

    OutGuide[texel] = float4(surface.P, surface.roughness);

    const float matte = MatteShare(surface.roughness);

    if (matte >= 1.0)
    {
        OutColor[texel] = constants.debug.x != 0u ? float4(0.0, 0.0, 0.0, 1.0) : (float4)0.0;

        return;
    }

    if (constants.frame.y == 0u)
    {
        SetWant(index, WANT_RAY, 0u, 0u, 0u);

        return;
    }

    const float3 surfaceNormal = LoadSurfaceNormal(pixel, surface.P, surface.N);
    const float width = constants.bricks.params.z;

    const uint level = LevelOf(surface.alpha);
    const bool isScattered = level < uint(SCATTER_BELOW_LEVEL);

    float3 T;
    float3 B;
    MakeBasis(surfaceNormal, T, B);

    // which crossing a pixel reads is moved, somewhere else every frame: over a few frames it has read each crossing around it
    // for as much as it is near it, which added up is the crossings blended
    float3 at = surface.P;
    float between = 1.0;

    if (constants.frame.z != 0u)
    {
        const uint own = PixelSeed(texel);
        const uint2 stepped = uint2(own, PcgHash(own)) + constants.frame.x * uint2(3242174889u, 2447445414u);
        const float2 shift = float2(UnitFloat(stepped.x), UnitFloat(stepped.y)) - 0.5;

        OutGuide[texel] = float4(surface.P, surface.roughness + GUIDE_DITHERED);

        between = BetweenMips(PcgHash(own ^ 0x9E3779B9u));

        // as far as the rays are scattered: where they are not, the crossings around have nothing more to say than the pixel's own
        at += (T * shift.x + B * shift.y) * width * (isScattered ? 2.0 * constants.aim.z : 1.0);
    }
    else
    {
        const float2 shift = float2(InterleavedGradientNoise(float2(texel) + 0.5), InterleavedGradientNoise(float2(texel.yx) + float2(47.5, 17.5))) - 0.5;

        at += (T * shift.x + B * shift.y) * width;
    }

    GlimmerBrickReceiver receiver = GlimmerFindBrickReceiver(constants.bricks, at, surfaceNormal);

    if (!receiver.found)
    {
        receiver = GlimmerFindBrickReceiver(constants.bricks, surface.P, surfaceNormal);
    }

    if (!receiver.found)
    {
        SetWant(index, WANT_RAY, 0u, 0u, 0u);

        return;
    }

    // something curved turns a reflection further across one crossing than its four bins reach
    if (((glimmerBrickMaterial[receiver.crossing] >> 29u) & GLIMMER_BRICK_FLAG_CURVED) != 0u || dot(surfaceNormal, receiver.normal) < CURVED_FACING)
    {
        SetWant(index, WANT_RAY, 0u, 0u, 0u);

        return;
    }

    int3 receiverCell = int3(0, 0, 0);
    uint receiverAxis = 0u;

    if (!LoadBrickPlace(receiver.brick, receiverCell, receiverAxis))
    {
        SetWant(index, WANT_RAY, 0u, 0u, 0u);

        return;
    }

    const GlimmerBrickCrossing crossing = GlimmerLoadBrickCrossing(constants.bricks, receiver.crossing);
    const float2 middleOffset = (GlimmerBrickMaskMiddle(crossing.mask) - 0.5) * width;
    const float2 slope = GlimmerBrickAcross(receiverAxis, crossing.normal) / GlimmerBrickAlong(receiverAxis, crossing.normal);

    const float3 receiverPosition = GlimmerBrickCellLow(constants.bricks, receiverCell)
        + GlimmerBrickPoint(receiverAxis, (float2(receiver.column) + 0.5) * width + middleOffset, crossing.depth - dot(slope, middleOffset));

    const uint block = slotBlockOf[receiver.crossing];

    const bool hasBlock = block < constants.limits.y
        && slotBlocks[block * BLOCK_WORDS] == receiver.crossing
        && slotBlocks[block * BLOCK_WORDS + 1u] == CrossingHash(receiver.crossing);

    uint heldCard[4] = { 0u, 0u, 0u, 0u };
    uint heldKey[4] = { 0u, 0u, 0u, 0u };

    if (hasBlock)
    {
        for (uint slot = 0; slot < 4u; slot++)
        {
            heldCard[slot] = slotBlocks[block * BLOCK_WORDS + 2u + 2u * slot];
            heldKey[slot] = slotBlocks[block * BLOCK_WORDS + 3u + 2u * slot];
        }
    }

    const int bins = int(1u << level);
    const float3 direction = Reflected(surface, surfaceNormal);

    // the bins are the crossing's, from where its middle sends the eye, so that every pixel on it reads the same ones
    const float3 fromMiddle = reflect(-normalize(camera.position.xyz - receiverPosition), receiver.normal);
    const float2 place = DirectionPlace(direction) * float(bins);
    const float2 middlePlace = DirectionPlace(fromMiddle) * float(bins);

    const int2 base = clamp(int2(floor(middlePlace - 0.5)), int2(0, 0), int2(bins - 2, bins - 2));
    const float2 ownPlace = place - 0.5 - float2(base);

    if (any(ownPlace < -STRAY) || any(ownPlace > 1.0 + STRAY))
    {
        SetWant(index, WANT_RAY, 0u, 0u, 0u);

        return;
    }

    // the cards are read along a way drawn from the lobe, as wide as the lobe is the narrow way; the bins are still those around the middle
    float3 aim = direction;
    float angle = constants.params.y * surface.alpha;

    float3 drawnAim = (float3)0.0;

    if (LobeAim(texel, surface, surfaceNormal, drawnAim))
    {
        aim = drawnAim;
        angle = constants.aim.x * surface.alpha * max(dot(surface.N, ViewOf(surface)), 0.1);
    }

    const float3 from = RayStart(receiverPosition, receiver.normal);

    float missing = 0.0;

    // the cards the slots hit and how far off along the pixel's own way, the nearest first, and whether any slot's ray left the bricks
    uint layerCard[4] = { 0u, 0u, 0u, 0u };
    float layerT[4] = { 0.0, 0.0, 0.0, 0.0 };
    uint numLayers = 0u;
    bool isOpen = false;

    for (uint tap = 0; tap < 4u; tap++)
    {
        const int2 bin = base + int2(tap & 1u, tap >> 1u);
        const uint key = BinKey(level, bin);

        uint held = 4u;

        for (uint heldSlot = 0; heldSlot < 4u; heldSlot++)
        {
            if ((heldKey[heldSlot] & 0xFFFFu) == key)
            {
                held = heldSlot;
            }
        }

        if (held == 4u)
        {
            missing += 1.0;

            continue;
        }

        const uint card = heldCard[held];

        if (card == SLOT_BELOW)
        {
            continue;
        }

        if (card == SLOT_SKY)
        {
            isOpen = true;

            continue;
        }

        int3 cardCell = int3(0, 0, 0);
        uint cardAxis = 0u;

        if (!LoadBrickPlace(card / uint(GLIMMER_BRICK_CARDS), cardCell, cardAxis))
        {
            missing += 1.0;

            continue;
        }

        // where the pixel's own way meets the plane across the card's axis through the slot's hit is where it would have landed on the same surface
        float t = f16tof32(heldKey[held] >> 16u);

        const float3 landed = from + BinDirection(level, bin, receiver.crossing) * t;
        const float aimAlong = GlimmerBrickAlong(cardAxis, aim);
        const float ownT = (GlimmerBrickAlong(cardAxis, landed) - GlimmerBrickAlong(cardAxis, surface.P)) / (abs(aimAlong) > 0.05 ? aimAlong : 1e9);

        if (ownT > 0.5 * t && ownT < 2.0 * t)
        {
            t = ownT;
        }

        uint layerPlace = numLayers;

        for (; layerPlace > 0u && layerT[layerPlace - 1u] > t; layerPlace--)
        {
            layerT[layerPlace] = layerT[layerPlace - 1u];
            layerCard[layerPlace] = layerCard[layerPlace - 1u];
        }

        layerT[layerPlace] = t;
        layerCard[layerPlace] = card;
        numLayers++;
    }

    // the slots say what is there to be seen, and the cards how much of it the pixel sees: the nearest card first, each taking its share
    // of what those in front left, and what is beyond the bricks the rest if any slot's ray found it
    float3 light = (float3)0.0;
    float hits = 0.0;
    float reach = 0.0;
    float remaining = 1.0;

    float3 readAt[4] = { (float3)0.0, (float3)0.0, (float3)0.0, (float3)0.0 };
    uint readAxis[4] = { 0u, 0u, 0u, 0u };

    for (uint layer = 0; layer < numLayers; layer++)
    {
        const uint card = layerCard[layer];

        int3 cardCell = int3(0, 0, 0);
        uint cardAxis = 0u;
        LoadBrickPlace(card / uint(GLIMMER_BRICK_CARDS), cardCell, cardAxis);

        readAt[layer] = surface.P + aim * layerT[layer];
        readAxis[layer] = cardAxis;

        // two slots that landed on one wall are one read
        bool isAgain = false;

        for (uint other = 0; other < layer; other++)
        {
            isAgain = isAgain || (card % uint(GLIMMER_BRICK_CARDS) == layerCard[other] % uint(GLIMMER_BRICK_CARDS)
                && cardAxis == readAxis[other]
                && all(abs(readAt[layer] - readAt[other]) < width));
        }

        if (isAgain || remaining < LAYER_LEAST)
        {
            continue;
        }

        const float4 arriving = CardLight(cardCell, cardAxis, card % uint(GLIMMER_BRICK_CARDS), readAt[layer], angle * layerT[layer] * between);

        if (arriving.a == 0.0)
        {
            continue;
        }

        light += arriving.rgb * remaining;
        hits += arriving.a * remaining;
        reach += layerT[layer] * arriving.a * remaining;
        remaining *= 1.0 - arriving.a;
    }

    const float beyond = isOpen ? remaining : 0.0;
    const uint blank = hits + beyond > LAYER_ENOUGH ? 0u : WANT_BLANK;

    if (missing > 0.0)
    {
        const uint code = (level << 28u) | (uint(base.y) << 13u) | uint(base.x);

        SetWant(index, receiver.crossing, code | blank, receiver.brick, uint(receiver.column.y) * uint(GLIMMER_BRICK_SIDE) + uint(receiver.column.x));
    }
    else if (blank != 0u)
    {
        SetWant(index, WANT_FILL, 0u, 0u, 0u);
    }

    if (blank != 0u)
    {
        Note(COUNT_BLANK_PIXELS);

        return;
    }

    // what is beyond the bricks is left to what the deferred pass has there
    const float total = max(hits + beyond, 1e-6);

    OutColor[texel] = float4(light, hits) * ((1.0 - matte) / total);

    Note(missing > 0.0 ? COUNT_PARTIAL_PIXELS : COUNT_SLOT_PIXELS);

    // green: all of it from slots, blue: some bins still to come
    if (constants.debug.x != 0u)
    {
        OutColor[texel] = missing > 0.0 ? float4(0.0, 0.0, 1.0, 1.0) : float4(0.0, 1.0, 0.0, 1.0);
    }
    OutInfo[texel] = float4(hits > 0.0 ? reach / hits : -1.0, 0.0, 0.0, 0.0);
}

#endif

#if defined(MODE_FINISH)

void ListPixel(uint index, uint single)
{
    uint at;
    InterlockedAdd(slotCounts[COUNT_LISTED], 1u, at);

    slotList[at] = index | single;
}

void Finish(int2 pixel, uint2 texel, uint index)
{
    const uint want = slotWanted[REQUEST_WORDS * index];

    if (want == WANT_NOTHING)
    {
        return;
    }

    if (want == WANT_RAY)
    {
        Note(COUNT_RAY_PIXELS);

        const ReflectionSurface surface = LoadSurface(pixel);
        const bool isSampled = constants.frame.w != 0u && surface.roughness >= SAMPLED_FROM;

        OutGuide[texel] = float4(surface.P, surface.roughness + (isSampled ? GUIDE_SAMPLED : 0.0));

        // every other one, in a checker that changes sides each frame, where a history holds the rest as they were
        if (isSampled && ((texel.x + texel.y + constants.frame.x) & 1u) != 0u)
        {
            OutInfo[texel] = float4(REACH_HOLE, 0.0, 0.0, 0.0);

            return;
        }

        ListPixel(index, 0u);

        return;
    }

    const uint code = slotWanted[REQUEST_WORDS * index + 1u];

    // a ray of its own meanwhile for a pixel with nothing to show
    if (want == WANT_FILL || (code & WANT_BLANK) != 0u)
    {
        ListPixel(index, LIST_SINGLE);
    }

    if (want == WANT_FILL)
    {
        return;
    }

    // a crossing is asked after once a frame however many pixels are on it
    const uint stamp = want ^ (constants.frame.x * 2654435769u);

    uint was;
    InterlockedExchange(slotClaims[PcgHash(want) & 0xFFFFFu], stamp, was);

    if (was == stamp)
    {
        return;
    }

    uint at;
    InterlockedAdd(slotCounts[COUNT_REQUESTS], 1u, at);

    if (at >= constants.limits.z)
    {
        return;
    }

    slotRequests[REQUEST_WORDS * at] = want;
    slotRequests[REQUEST_WORDS * at + 1u] = code & ~WANT_BLANK;
    slotRequests[REQUEST_WORDS * at + 2u] = slotWanted[REQUEST_WORDS * index + 2u];
    slotRequests[REQUEST_WORDS * at + 3u] = slotWanted[REQUEST_WORDS * index + 3u];
}

#endif

#if defined(MODE_RAYS)

void TraceListed(uint listed)
{
    if (listed >= slotCounts[COUNT_LISTED])
    {
        return;
    }

    const uint entry = slotList[listed];
    const uint place = entry & ~LIST_SINGLE;
    const uint2 texel = uint2(place % constants.dimensions.z, place / constants.dimensions.z);
    const int2 pixel = min(int2(texel) * 2, int2(constants.dimensions.xy) - 1);

    const ReflectionSurface surface = LoadSurface(pixel);
    const float3 surfaceNormal = LoadSurfaceNormal(pixel, surface.P, surface.N);
    const float matte = MatteShare(surface.roughness);

    // red: a ray of its own, magenta: one filling in for what its slots lack
    if (constants.debug.x != 0u)
    {
        OutColor[texel] = (entry & LIST_SINGLE) != 0u ? float4(1.0, 0.0, 1.0, 1.0) : float4(1.0, 0.0, 0.0, 1.0);

        return;
    }

    const float3 origin = RayStart(surface.P, surfaceNormal);

    float3 direction = Reflected(surface, surfaceNormal);
    float angle = constants.params.y * surface.alpha;

    float3 drawnAim = (float3)0.0;

    if ((entry & LIST_SINGLE) != 0u)
    {
        // filling in for what the pixel's slots lack: the way its cards were read along
        if (LobeAim(texel, surface, surfaceNormal, drawnAim))
        {
            direction = drawnAim;
            angle = constants.aim.x * surface.alpha * max(dot(surface.N, ViewOf(surface)), 0.1);
        }
    }
    else if (constants.frame.w != 0u && surface.roughness >= SAMPLED_FROM)
    {
        // somewhere on a disc of the lobe: a place of the pixel's own, and from there a step on every frame it traces in
        const uint own = PixelSeed(texel);
        const uint traced = constants.frame.x >> 1u;

        const float turn = 2.0 * HYP_FMATH_PI * UnitFloat(PcgHash(own) + traced * 3242174889u);
        const float radius = SPREAD_DISC * surface.alpha * sqrt(UnitFloat(own + traced * 2447445414u));

        float3 T;
        float3 B;
        MakeBasis(direction, T, B);

        direction = normalize(direction + T * (radius * cos(turn)) + B * (radius * sin(turn)));
        direction = normalize(direction + surfaceNormal * max(HORIZON - dot(direction, surfaceNormal), 0.0));

        angle = 2.0 * SPREAD_DISC * surface.alpha * BetweenMips(PcgHash(own ^ 0x9E3779B9u));
    }

    GlimmerBrickHit hit;

    if (!TraceGlimmerBricks(constants.bricks, origin, direction, 0.0, constants.params.z, hit))
    {
        OutColor[texel] = (float4)0.0;
        OutInfo[texel] = float4(-1.0, 0.0, 0.0, 0.0);

        return;
    }

    float4 light = (float4)0.0;

    if (dot(hit.normal, direction) < 0.0)
    {
        light = HitLight(hit, origin + direction * hit.t, angle * hit.t);
    }

    const float coverage = light.a > 0.0 ? 1.0 - matte : 0.0;

    OutColor[texel] = float4(light.rgb / max(light.a, 1e-6) * coverage, coverage);
    OutInfo[texel] = float4(hit.t, 0.0, 0.0, 0.0);
}

#endif

#if defined(MODE_WHOLE_LIST)

void ListWhole(uint2 pixel)
{
    if (all((pixel & 1u) == 0u))
    {
        return;
    }

    const ReflectionSurface surface = LoadSurface(int2(pixel));

    if (!surface.isValid || surface.roughness >= SAMPLED_FROM)
    {
        return;
    }

    // the guide has the block's resolved pixel unmarked where it traced along the middle of its lobe
    const float4 resolved = GuideTexture.Load(int3(min(pixel / 2u, constants.dimensions.zw - 1u), 0));

    if (resolved.a < 0.0 || abs(resolved.a - surface.roughness) > ROUGHNESS_TOLERANCE)
    {
        return;
    }

    uint at;
    InterlockedAdd(slotCounts[COUNT_WHOLE], 1u, at);

    slotWhole[at] = pixel.y * constants.dimensions.x + pixel.x;
}

#endif

#if defined(MODE_WHOLE_RAYS)

void TraceWhole(uint listed)
{
    if (listed >= slotCounts[COUNT_WHOLE])
    {
        return;
    }

    const uint place = slotWhole[listed];
    const int2 pixel = int2(place % constants.dimensions.x, place / constants.dimensions.x);

    const ReflectionSurface surface = LoadSurface(pixel);
    const float3 surfaceNormal = LoadSurfaceNormal(pixel, surface.P, surface.N);

    const float3 origin = RayStart(surface.P, surfaceNormal);
    const float3 direction = Reflected(surface, surfaceNormal);

    GlimmerBrickHit hit;
    float4 light = (float4)0.0;

    if (TraceGlimmerBricks(constants.bricks, origin, direction, 0.0, constants.params.z, hit) && dot(hit.normal, direction) < 0.0)
    {
        light = HitLight(hit, origin + direction * hit.t, constants.params.y * surface.alpha * hit.t);
    }

    const float coverage = light.a > 0.0 ? 1.0 - MatteShare(surface.roughness) : 0.0;

    OutColor[pixel] = float4(light.rgb / max(light.a, 1e-6) * coverage, coverage);
}

#endif

#if defined(MODE_HISTORY)

bool PreviousScreen(float3 position, out float2 outScreen)
{
    const float4 clip = mul(camera.prevViewProjMat, float4(position, 1.0));

    outScreen = (float2)0.0;

    if (clip.w <= 0.0)
    {
        return false;
    }

    const float2 ndc = clip.xy / clip.w;

    outScreen = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5) * float2(constants.dimensions.xy);

    return all(abs(ndc) <= 1.0);
}

// what was added up at a place on last frame's screen from the four texels around it that have this pixel's surface, with the frames behind it in outFrames
float4 AddedAt(float2 screen, bool isOnScreen, float3 position, float slack, bool isAnywhere, float3 plane, float roughness, out float outFrames, out float outFound)
{
    outFrames = 0.0;
    outFound = 0.0;

    if (!isOnScreen)
    {
        return (float4)0.0;
    }

    const float2 at = (screen - 0.5) * 0.5;
    const int2 corner = int2(floor(at));
    const float2 fraction = at - float2(corner);

    float4 sum = (float4)0.0;

    [unroll]
    for (uint tap = 0; tap < 4u; tap++)
    {
        const int2 offset = int2(tap & 1u, tap >> 1u);
        const int2 texel = corner + offset;

        if (any(texel < 0) || any(texel >= int2(constants.dimensions.zw)))
        {
            continue;
        }

        const float4 there = HistoryGuideTexture.Load(int3(texel, 0));

        if (there.a < 0.5 * GUIDE_SAMPLED)
        {
            continue;
        }

        if (isAnywhere)
        {
            const float finish = there.a - (there.a >= 1.5 * GUIDE_SAMPLED ? GUIDE_DITHERED : GUIDE_SAMPLED);

            if (abs(dot(there.xyz - position, plane)) > slack || abs(finish - roughness) > ROUGHNESS_TOLERANCE)
            {
                continue;
            }
        }
        else if (distance(there.xyz, position) > slack)
        {
            continue;
        }

        const float2 weights = lerp(1.0 - fraction, fraction, float2(offset));
        const float weight = weights.x * weights.y;

        sum += HistoryColorTexture.Load(int3(texel, 0)) * weight;
        outFrames += HistoryInfoTexture.Load(int3(texel, 0)).y * weight;
        outFound += weight;
    }

    if (outFound < 1e-3)
    {
        outFound = 0.0;
        outFrames = 0.0;

        return (float4)0.0;
    }

    outFrames /= outFound;

    return sum / outFound;
}

void AddUp(uint2 texel)
{
    const int2 halfPixel = int2(texel);

    float4 color = ColorTexture.Load(int3(halfPixel, 0));
    float reach = InfoTexture.Load(int3(halfPixel, 0)).x;

    const float4 here = GuideTexture.Load(int3(halfPixel, 0));

    // a pixel with no ray this frame keeps what it has added up, and failing that takes what the four beside it traced
    const bool isHole = reach == REACH_HOLE;

    if (isHole)
    {
        float4 beside = (float4)0.0;
        float besideWeight = 0.0;

        [unroll]
        for (int neighbour = 0; neighbour < 4; neighbour++)
        {
            const int2 offset = int2((neighbour & 1) * (1 - (neighbour & 2)), ((neighbour + 1) & 1) * (1 - (neighbour & 2)));
            const int2 tap = clamp(halfPixel + offset, int2(0, 0), int2(constants.dimensions.zw) - 1);

            if (InfoTexture.Load(int3(tap, 0)).x != REACH_HOLE)
            {
                beside += ColorTexture.Load(int3(tap, 0));
                besideWeight += 1.0;
            }
        }

        color = beside / max(besideWeight, 1.0);
        reach = 0.0;
    }

    OutColor[texel] = color;
    OutInfo[texel] = float4(reach, 0.0, 0.0, 0.0);

    if (here.a < 0.5 * GUIDE_SAMPLED || constants.limits.w != 0u)
    {
        return;
    }

    // from the depth buffer and not from the guide, whose positions are half floats
    const int2 pixel = min(halfPixel * 2, int2(constants.dimensions.xy) - 1);
    const ReflectionSurface surface = LoadSurface(pixel);

    float2 screen = (float2)0.0;
    const bool isOnScreen = PreviousScreen(surface.P, screen);

    const float slack = SAME_SURFACE * distance(camera.position.xyz, surface.P) + 1e-3;
    const bool isDithered = here.a >= 1.5 * GUIDE_SAMPLED;
    const float roughness = here.a - (isDithered ? GUIDE_DITHERED : GUIDE_SAMPLED);

    // where what the pixel reflects was: along the way the pixel is seen, as far behind the surface as its rays went
    float3 plane = (float3)0.0;
    float2 image = screen;
    bool isImageOnScreen = isOnScreen;

    float reflected = 0.0;
    float forgets = 0.0;

    if (!isHole && reach != 0.0)
    {
        const float went = reach < 0.0 ? MISS_DISTANCE : reach;
        const float3 toSurface = surface.P - camera.position.xyz;
        const float away = length(toSurface);

        isImageOnScreen = PreviousScreen(camera.position.xyz + toSurface / away * (away + went), image);

        const float mirrorlike = 1.0 - smoothstep(VIRTUAL_BELOW, SURFACE_ABOVE, roughness);

        forgets = SLIDE_FORGETS * distance(image, screen) * mirrorlike;

        // only something flat has its reflection elsewhere on itself, and only where no normal map turns it by more than its lobe is wide
        if (isDithered)
        {
            plane = LoadSurfaceNormal(pixel, surface.P, surface.N);

            const float lobe = 2.0 * roughness * roughness;

            reflected = mirrorlike * (1.0 - smoothstep(0.1 * lobe, 0.5 * lobe, length(surface.N - plane)));
        }
    }

    float4 sum = (float4)0.0;
    float frames = 0.0;
    float total = 0.0;
    float bySurface = 1.0;

    if (reflected > 0.0)
    {
        sum = AddedAt(image, isImageOnScreen, surface.P, slack, true, plane, roughness, frames, total);
        bySurface = total > 0.0 ? 1.0 - reflected : 1.0;
    }

    if (reflected < 1.0 || total == 0.0)
    {
        float ownFrames = 0.0;
        float found = 0.0;
        const float4 ownSum = AddedAt(screen, isOnScreen, surface.P, slack, false, plane, roughness, ownFrames, found);

        if (total == 0.0)
        {
            sum = ownSum;
            frames = ownFrames;
            total = found;
        }
        else if (found > 0.0)
        {
            sum = lerp(ownSum, sum, reflected);
            frames = lerp(ownFrames, frames, reflected);
        }
        else
        {
            bySurface = 0.0;
        }
    }

    if (total == 0.0)
    {
        OutInfo[texel] = float4(reach, 1.0, 0.0, 0.0);

        return;
    }

    frames /= 1.0 + forgets * bySurface;

    if (isHole)
    {
        OutColor[texel] = sum;
        OutInfo[texel] = float4(0.0, frames, 0.0, 0.0);

        return;
    }

    const float added = min(frames + 1.0, float(isDithered ? constants.frame.z : constants.frame.w));

    // what is many times what the pixel has added up is dimmed to that: one ray that finds a window is a flash
    float4 fresh = color;

    if (constants.aim.y > 0.0 && frames >= 4.0)
    {
        const float most = constants.aim.y * dot(sum.rgb, float3(0.2126, 0.7152, 0.0722)) + 1e-3;
        const float luminance = dot(fresh.rgb, float3(0.2126, 0.7152, 0.0722));

        fresh.rgb *= min(1.0, most / max(luminance, 1e-6));
    }

    OutColor[texel] = lerp(sum, fresh, 1.0 / max(added, 1.0));
    OutInfo[texel] = float4(reach, added, 0.0, 0.0);
}

#endif

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
#if defined(MODE_REQUESTS)
    TraceRequest(dispatchThreadId.y * TRACE_ROW + dispatchThreadId.x);
#elif defined(MODE_RAYS)
    TraceListed(dispatchThreadId.y * TRACE_ROW + dispatchThreadId.x);
#elif defined(MODE_WHOLE_LIST)
    if (all(dispatchThreadId.xy < constants.dimensions.xy))
    {
        ListWhole(dispatchThreadId.xy);
    }
#elif defined(MODE_WHOLE_RAYS)
    TraceWhole(dispatchThreadId.y * TRACE_ROW + dispatchThreadId.x);
#elif defined(MODE_SPREAD)
    const int2 pixel = int2(dispatchThreadId.xy);

    if (any(dispatchThreadId.xy >= constants.dimensions.xy))
    {
        return;
    }

    const ReflectionSurface surface = LoadSurface(pixel);

    if (!surface.isValid)
    {
        OutColor[pixel] = (float4)0.0;

        return;
    }

    const float slack = SPREAD_PLANE_TOLERANCE * distance(camera.position.xyz, surface.P) + 1e-3;
    const int2 halfDimensions = int2(constants.dimensions.zw);

    // those alike, and all of them in case none is
    const int2 base = (pixel + 2) / 2 - 1;

    float4 alike = (float4)0.0;
    float alikeWeight = 0.0;
    float4 anyOf = (float4)0.0;
    float anyWeight = 0.0;

    [unroll]
    for (uint corner = 0; corner < 4u; corner++)
    {
        const int2 block = base + int2(corner & 1u, corner >> 1u);

        if (any(block < 0) || any(block >= halfDimensions))
        {
            continue;
        }

        const int2 source = min(block * 2, int2(constants.dimensions.xy) - 1);
        const float2 apart = 1.0 - min(abs(float2(pixel - source)) * 0.5, 1.0);

        float weight = apart.x * apart.y;

        if (weight <= 0.0)
        {
            continue;
        }

        const float4 traced = ColorTexture.Load(int3(block, 0));

        anyOf += traced * weight;
        anyWeight += weight;

        if (any(source != pixel))
        {
            const ReflectionSurface other = LoadSurface(source);

            if (!other.isValid)
            {
                continue;
            }

            const float off = dot(other.P - surface.P, surface.N) / slack;
            const float finish = (other.roughness - surface.roughness) / ROUGHNESS_TOLERANCE;

            weight *= pow(saturate(dot(other.N, surface.N)), 32.0) * exp2(-off * off - finish * finish);
        }

        alike += traced * weight;
        alikeWeight += weight;
    }

    OutColor[pixel] = alikeWeight > 0.05 ? alike / alikeWeight : anyOf / max(anyWeight, 1e-6);
#else
    const uint2 texel = dispatchThreadId.xy;

    if (any(texel >= constants.dimensions.zw))
    {
        return;
    }

    const int2 pixel = min(int2(texel) * 2, int2(constants.dimensions.xy) - 1);
    const uint index = texel.y * constants.dimensions.z + texel.x;

#if defined(MODE_RESOLVE)
    if (index == 0u)
    {
        slotCounts[COUNT_REQUESTS] = 0u;
        slotCounts[COUNT_LISTED] = 0u;
        slotCounts[COUNT_WHOLE] = 0u;
        slotCounts[COUNT_SLOT_PIXELS] = 0u;
        slotCounts[COUNT_PARTIAL_PIXELS] = 0u;
        slotCounts[COUNT_BLANK_PIXELS] = 0u;
        slotCounts[COUNT_RAY_PIXELS] = 0u;
    }

    Resolve(pixel, texel, index);
#elif defined(MODE_FINISH)
    Finish(pixel, texel, index);
#else
    AddUp(texel);
#endif
#endif
}
