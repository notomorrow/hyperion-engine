#ifndef GLIMMER_BRICKS_HLSLI
#define GLIMMER_BRICKS_HLSLI

/// static geometry approximated by surface crossings along columns.
/// up to three bricks in a *cell*. one per axis, a triangle in the one its normal is nearest
/// --
/// expects glimmerBrickTable, glimmerBrickIndex and glimmerBrickCrossings to be declared, unless GLIMMER_BRICKS_NO_TRACE

/////////////////////
///// Constants /////
/////////////////////

// NOTE must match GlimmerBrickWorld.hpp
#define MAX_BATCH_CHUNKS 32
#define GLIMMER_BRICK_GRID_XZ 128
#define GLIMMER_BRICK_GRID_Y 64
#define GLIMMER_BRICK_CHUNK_CELLS 8
#define GLIMMER_BRICK_SIDE 32
#define GLIMMER_BRICK_TILE 4
#define GLIMMER_BRICK_TILES_PER_SIDE (GLIMMER_BRICK_SIDE / GLIMMER_BRICK_TILE)
#define GLIMMER_BRICK_MAX_COLUMN_CROSSINGS 8
#define GLIMMER_BRICK_PAGE_CROSSINGS 256
#define GLIMMER_BRICK_MAX_PAGES 31

/// a card is the crossings of a brick facing one way along its axis within a quarter of its depth.
#define GLIMMER_BRICK_LAYERS 4
#define GLIMMER_BRICK_CARDS (2 * GLIMMER_BRICK_LAYERS)
#define GLIMMER_BRICK_MIPS 6
#define GLIMMER_BRICK_CARD_FIRST_MIP 2
#define GLIMMER_BRICK_CARD_SIDE (GLIMMER_BRICK_SIDE >> GLIMMER_BRICK_CARD_FIRST_MIP)
#define GLIMMER_BRICK_CARD_TEXELS ((4 * GLIMMER_BRICK_CARD_SIDE * GLIMMER_BRICK_CARD_SIDE - 1) / 3)

// per brick in glimmerBrickIndex: its pages of crossings (count first), its cards, where it is, how many times it has been taken, then a depth range per tile, then first crossing and count per column
#define GLIMMER_BRICK_INDEX_CARDS (1 + GLIMMER_BRICK_MAX_PAGES)
#define GLIMMER_BRICK_INDEX_CARD_MASK (GLIMMER_BRICK_INDEX_CARDS + GLIMMER_BRICK_CARDS)
#define GLIMMER_BRICK_INDEX_CELL (GLIMMER_BRICK_INDEX_CARD_MASK + 1)
#define GLIMMER_BRICK_INDEX_AXIS (GLIMMER_BRICK_INDEX_CELL + 3) // GLIMMER_BRICK_NONE while the brick is free
#define GLIMMER_BRICK_INDEX_SERIAL (GLIMMER_BRICK_INDEX_AXIS + 1)
#define GLIMMER_BRICK_INDEX_TILES (GLIMMER_BRICK_INDEX_SERIAL + 1)
#define GLIMMER_BRICK_INDEX_COLUMNS (GLIMMER_BRICK_INDEX_TILES + GLIMMER_BRICK_TILES_PER_SIDE * GLIMMER_BRICK_TILES_PER_SIDE)
#define GLIMMER_BRICK_INDEX_STRIDE (GLIMMER_BRICK_INDEX_COLUMNS + GLIMMER_BRICK_SIDE * GLIMMER_BRICK_SIDE)

#define GLIMMER_BRICK_NONE 0xFFFFFFFFu
#define GLIMMER_BRICK_MISS_DISTANCE 3.402823466e+38f

// NOTE must match GlimmerBrickWorld.hpp
#define GLIMMER_BRICK_CARD_BITS 18u
#define GLIMMER_BRICK_SERIAL_MASK 0x1FFFu

#define GLIMMER_BRICK_FLAG_CURVED 2u
#define GLIMMER_BRICK_FLAG_DOUBLE_SIDED 4u

// columns beyond its own that a crossing's plane still stops a ray
#define GLIMMER_BRICK_SLACK 0.25

// the build's counters: the counters, then each slot's, each slot's list heads, then the batch's build list of two words a brick (NOTE must match GlimmerBrickWorld.cpp)
#define COUNTER_FREE_BRICKS 0
#define COUNTER_LIST_NODES 1
#define COUNTER_BUILD_BRICKS 2
#define COUNTER_FAILED_BRICKS 3
#define COUNTER_DROPPED_COLUMNS 4
#define COUNTER_DROPPED_NODES 5
#define COUNTER_FREE_PAGES 6
#define COUNTER_FREE_CARDS 7
#define COUNTER_LIVE_BRICKS 14
#define COUNTER_BATCH_DROPPED_NODES 15
#define NUM_COUNTERS 16

#define SLOT_FAILED_BRICKS 0
#define SLOT_FAILED_PAGES 1
#define SLOT_FAILED_CARDS 2
#define SLOT_BRICKS 3
#define SLOT_PAGES 4
#define SLOT_CARDS 5
#define SLOT_COUNTERS 8

#define COLUMNS_PER_BRICK (GLIMMER_BRICK_SIDE * GLIMMER_BRICK_SIDE)
#define BOX_BRICKS uint(GLIMMER_BRICK_CHUNK_CELLS * GLIMMER_BRICK_CHUNK_CELLS * GLIMMER_BRICK_CHUNK_CELLS * 3)

#define SLOT_COUNTERS_BASE uint(NUM_COUNTERS)
#define LIST_HEADS_BASE (SLOT_COUNTERS_BASE + uint(MAX_BATCH_CHUNKS * SLOT_COUNTERS))
#define BUILD_LIST_BASE (LIST_HEADS_BASE + uint(MAX_BATCH_CHUNKS) * BOX_BRICKS)

/////////////////////

static const int3 GlimmerBrickGridSize = int3(GLIMMER_BRICK_GRID_XZ, GLIMMER_BRICK_GRID_Y, GLIMMER_BRICK_GRID_XZ);

struct GlimmerBrickParams
{
    int4 origin;   // xyz = absolute cell of the window's first cell, w = 1 once the window is valid
    float4 params; // x = cell size, y = 1 / cell size, z = column width, w = 1 / column width
};

float GlimmerBrickDepthMargin(GlimmerBrickParams bricks)
{
    return 2.0 * bricks.params.z;
}

float GlimmerBrickDepthSpan(GlimmerBrickParams bricks)
{
    return bricks.params.x + 2.0 * GlimmerBrickDepthMargin(bricks);
}

uint GlimmerBrickPackDepth(GlimmerBrickParams bricks, float depth)
{
    return uint(saturate((depth + GlimmerBrickDepthMargin(bricks)) / GlimmerBrickDepthSpan(bricks)) * 65535.0 + 0.5);
}

float GlimmerBrickUnpackDepth(GlimmerBrickParams bricks, uint code)
{
    return float(code) / 65535.0 * GlimmerBrickDepthSpan(bricks) - GlimmerBrickDepthMargin(bricks);
}

/// get the dom axis of the normal.
uint GlimmerBrickAxis(float3 normal)
{
    const float3 a = abs(normal);

    return a.x >= a.y && a.x >= a.z ? 0u : (a.y >= a.z ? 1u : 2u);
}

/// octahedral normal packed
uint GlimmerBrickPackNormal(float3 n)
{
    n /= abs(n.x) + abs(n.y) + abs(n.z);

    float2 e = n.xy;

    if (n.z < 0.0)
    {
        e = (1.0 - abs(n.yx)) * select(n.xy >= 0.0, 1.0, -1.0);
    }

    const uint2 q = uint2(round(e * 127.0) + 127.0);

    return q.x | (q.y << 8u);
}

float3 GlimmerBrickUnpackNormal(uint packed)
{
    const float2 e = (float2(float(packed & 0xFFu), float((packed >> 8u) & 0xFFu)) - 127.0) / 127.0;

    float3 n = float3(e, 1.0 - abs(e.x) - abs(e.y));

    const float t = max(-n.z, 0.0);
    n.xy += select(n.xy >= 0.0, -t, t);

    return normalize(n);
}

uint GlimmerBrickPackColor(float3 color)
{
    const uint3 q = uint3(saturate(color) * float3(31.0, 63.0, 31.0) + 0.5);

    return q.r | (q.g << 5u) | (q.b << 11u);
}

float3 GlimmerBrickUnpackColor(uint packed)
{
    return float3(float(packed & 31u), float((packed >> 5u) & 63u), float((packed >> 11u) & 31u)) / float3(31.0, 63.0, 31.0);
}

/// a crossing's word in glimmerBrickMaterial: material, then finish, then flags
bool GlimmerBrickHasFlag(uint material, uint flag)
{
    return ((material >> 29u) & flag) != 0u;
}

float3 GlimmerBrickPoint(uint axis, float2 across, float depth)
{
    return select(axis == 0u, float3(depth, across.x, across.y), select(axis == 1u, float3(across.y, depth, across.x), float3(across.x, across.y, depth)));
}

float2 GlimmerBrickAcross(uint axis, float3 p)
{
    return select(axis == 0u, p.yz, select(axis == 1u, p.zx, p.xy));
}

float GlimmerBrickAlong(uint axis, float3 p)
{
    return select(axis == 0u, p.x, select(axis == 1u, p.y, p.z));
}

bool GlimmerBrickCellInWindow(GlimmerBrickParams bricks, int3 cell)
{
    return bricks.origin.w != 0 && all(cell >= bricks.origin.xyz) && all(cell < bricks.origin.xyz + GlimmerBrickGridSize);
}

uint GlimmerBrickTableIndex(int3 cell, uint axis)
{
    const uint3 wrapped = uint3(cell & (GlimmerBrickGridSize - 1));

    return ((wrapped.z * uint(GLIMMER_BRICK_GRID_Y) + wrapped.y) * uint(GLIMMER_BRICK_GRID_XZ) + wrapped.x) * 3u + axis;
}

float3 GlimmerBrickCellLow(GlimmerBrickParams bricks, int3 cell)
{
    return float3(cell) * bricks.params.x;
}

/// where a word of a brick's header is in glimmerBrickIndex, `field` being a GLIMMER_BRICK_INDEX_ offset
uint GlimmerBrickWord(uint brick, uint field)
{
    return brick * uint(GLIMMER_BRICK_INDEX_STRIDE) + field;
}

uint GlimmerBrickTileIndex(uint brick, int2 tile)
{
    return GlimmerBrickWord(brick, uint(GLIMMER_BRICK_INDEX_TILES) + uint(tile.y) * uint(GLIMMER_BRICK_TILES_PER_SIDE) + uint(tile.x));
}

uint GlimmerBrickColumnIndex(uint brick, int2 column)
{
    return GlimmerBrickWord(brick, uint(GLIMMER_BRICK_INDEX_COLUMNS) + uint(column.y) * uint(GLIMMER_BRICK_SIDE) + uint(column.x));
}

// the middle of what a crossing covers of its column, across it
float2 GlimmerBrickMaskMiddle(uint mask)
{
    float2 sum = (float2)0.0;

    for (uint part = 0; part < 16u; part++)
    {
        if ((mask >> part) & 1u)
        {
            sum += float2(float(part & 3u), float(part >> 2u)) + 0.5;
        }
    }

    return sum / (4.0 * float(max(countbits(mask), 1u)));
}

/// which card of its brick a crossing is on.
/// by the way it faces along the axis, then by the layer its depth is in
uint GlimmerBrickCardSlot(GlimmerBrickParams bricks, uint axis, float3 normal, float depth)
{
    const uint facing = GlimmerBrickAlong(axis, normal) > 0.0 ? 1u : 0u;
    const uint layer = uint(clamp(depth * bricks.params.y * float(GLIMMER_BRICK_LAYERS), 0.0, float(GLIMMER_BRICK_LAYERS) - 0.5));

    return facing * uint(GLIMMER_BRICK_LAYERS) + layer;
}

/// `level` is of the brick's columns, from GLIMMER_BRICK_CARD_FIRST_MIP
uint GlimmerBrickCardMipOffset(uint level)
{
    const uint side = uint(GLIMMER_BRICK_CARD_SIDE);
    const uint left = side >> (level - uint(GLIMMER_BRICK_CARD_FIRST_MIP));

    return (4u * side * side - 4u * left * left) / 3u;
}

/// radiance as 8 bits a channel under a shared exponent of 5, and in 3 bits how much of the texel is surface
/// 0 == nothing is there
uint GlimmerBrickPackLight(float3 light, uint share)
{
    light = clamp(light, 0.0, 60000.0);

    const float most = max(light.r, max(light.g, light.b));
    const int exponent = clamp(int(ceil(log2(max(most, 1e-9)))) + 15, 0, 31);
    const uint3 q = uint3(min(round(light * (exp2(float(15 - exponent)) * 255.0)), 255.0));

    return q.r | (q.g << 8u) | (q.b << 16u) | (uint(exponent) << 24u) | (share << 29u);
}

float4 GlimmerBrickUnpackLight(uint packed)
{
    const float scale = exp2(float((packed >> 24u) & 31u) - 15.0) / 255.0;

    return float4(float3(float(packed & 0xFFu), float((packed >> 8u) & 0xFFu), float((packed >> 16u) & 0xFFu)) * scale, float(packed >> 29u) / 7.0);
}

float3 GlimmerBrickSafeDirection(float3 direction)
{
    return select(abs(direction) > 1e-12, direction, select(direction >= 0.0, 1e-12, -1e-12));
}

// x = where a ray comes into the window, y = where it leaves
float2 GlimmerBrickWindowRange(GlimmerBrickParams bricks, float3 origin, float3 direction)
{
    const float3 low = float3(bricks.origin.xyz) * bricks.params.x;
    const float3 high = low + float3(GlimmerBrickGridSize) * bricks.params.x;

    const float3 inverseDirection = 1.0 / GlimmerBrickSafeDirection(direction);
    const float3 t0 = (low - origin) * inverseDirection;
    const float3 t1 = (high - origin) * inverseDirection;
    const float3 tNear = min(t0, t1);
    const float3 tFar = max(t0, t1);

    return float2(max(tNear.x, max(tNear.y, tNear.z)), min(tFar.x, min(tFar.y, tFar.z)));
}

// how far along a ray from inside the window it leaves it, 0 for one that starts outside or with no window
float GlimmerBrickWindowExit(GlimmerBrickParams bricks, float3 origin, float3 direction)
{
    const float2 range = GlimmerBrickWindowRange(bricks, origin, direction);

    return bricks.origin.w == 0 || range.x > 0.0 ? 0.0 : range.y;
}

#ifndef GLIMMER_BRICKS_NO_TRACE

struct GlimmerBrickCrossing
{
    ///// depth from the cell's low side along the brick's axis at the middle of the column
    float depth;
    float3 normal;
    uint mask;   // visibility bitmask
    float3 color;
};

GlimmerBrickCrossing GlimmerLoadBrickCrossing(GlimmerBrickParams bricks, uint crossingIndex)
{
    const uint first = glimmerBrickCrossings[2u * crossingIndex];
    const uint second = glimmerBrickCrossings[2u * crossingIndex + 1u];

    GlimmerBrickCrossing crossing;
    crossing.depth = GlimmerBrickUnpackDepth(bricks, first & 0xFFFFu);
    crossing.normal = GlimmerBrickUnpackNormal(first >> 16u);
    crossing.mask = second & 0xFFFFu;
    crossing.color = GlimmerBrickUnpackColor(second >> 16u);

    return crossing;
}

// false while the brick is free
bool GlimmerLoadBrickPlace(uint brick, out int3 outCell, out uint outAxis)
{
    const uint header = GlimmerBrickWord(brick, 0u);

    outAxis = glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_AXIS)];
    outCell = int3(
        int(glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CELL)]),
        int(glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CELL) + 1u]),
        int(glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CELL) + 2u]));

    return outAxis <= 2u;
}

// how many crossings a column has, from outFirst on
uint GlimmerLoadBrickColumn(uint brick, int2 column, out uint outFirst)
{
    const uint entry = glimmerBrickIndex[GlimmerBrickColumnIndex(brick, column)];

    outFirst = entry & 0xFFFFFFu;

    return min(entry >> 24u, uint(GLIMMER_BRICK_MAX_COLUMN_CROSSINGS));
}

// a crossing's surface at the middle of what it covers of its column
float3 GlimmerBrickCrossingPoint(GlimmerBrickParams bricks, int3 cell, uint axis, int2 column, GlimmerBrickCrossing crossing)
{
    const float width = bricks.params.z;
    const float2 offset = (GlimmerBrickMaskMiddle(crossing.mask) - 0.5) * width;
    const float2 slope = GlimmerBrickAcross(axis, crossing.normal) / GlimmerBrickAlong(axis, crossing.normal);

    return GlimmerBrickCellLow(bricks, cell) + GlimmerBrickPoint(axis, (float2(column) + 0.5) * width + offset, crossing.depth - dot(slope, offset));
}

struct GlimmerBrickHit
{
    float t;
    uint crossing;
    uint brick;
    uint axis;
    float3 normal;
    float depth;
    uint steps; // cells and columns looked at on the way
};

void GlimmerWalkBrick(
    GlimmerBrickParams bricks,
    uint brick,
    uint axis,
    float3 cellLow,
    float3 origin,
    float3 direction,
    float tIn,
    float tOut,
    float tMin,
    inout GlimmerBrickHit hit)
{
    const float width = bricks.params.z;
    const int side = GLIMMER_BRICK_SIDE;

    const float3 local = origin - cellLow;

    // across the columns, in columns, and along them, in metres
    const float2 originAcross = GlimmerBrickAcross(axis, local) * bricks.params.w;
    const float2 directionAcross = GlimmerBrickAcross(axis, direction) * bricks.params.w;
    const float originAlong = GlimmerBrickAlong(axis, local);
    const float directionAlong = GlimmerBrickAlong(axis, direction);

    const bool2 moves = abs(directionAcross) > 1e-12;
    const float fastest = max(max(abs(directionAcross.x), abs(directionAcross.y)), 1e-3);

    const float nudge = 1e-3 / fastest;
    const float slack = min(GLIMMER_BRICK_SLACK / fastest, 2.0 * GLIMMER_BRICK_SLACK * width);

    float t = tIn;

    uint heldTile = GLIMMER_BRICK_NONE;
    uint range = 0u;

    [loop]
    for (int step = 0; step < 2 * side + 4 && t < tOut && t < hit.t; step++)
    {
        hit.steps++;

        const float2 at = originAcross + directionAcross * (t + nudge);
        const int2 column = clamp(int2(floor(at)), int2(0, 0), int2(side - 1, side - 1));

        const int2 tile = column / GLIMMER_BRICK_TILE;
        const uint tileIndex = GlimmerBrickTileIndex(brick, tile);

        if (tileIndex != heldTile)
        {
            heldTile = tileIndex;
            range = glimmerBrickIndex[tileIndex];
        }

        const float2 tileEdge = float2(tile * GLIMMER_BRICK_TILE) + select(directionAcross > 0.0, float(GLIMMER_BRICK_TILE), 0.0);
        const float2 tileT = select(moves, (tileEdge - originAcross) / directionAcross, (float2)GLIMMER_BRICK_MISS_DISTANCE);
        const float tTile = min(min(tileT.x, tileT.y), tOut);

        const uint nearest = range & 0xFFFFu;
        const uint furthest = range >> 16u;

        const float depthIn = originAlong + directionAlong * t;
        const float depthOut = originAlong + directionAlong * tTile;

        if (nearest > furthest
            || max(depthIn, depthOut) < GlimmerBrickUnpackDepth(bricks, nearest) - width
            || min(depthIn, depthOut) > GlimmerBrickUnpackDepth(bricks, furthest) + width)
        {
            t = max(tTile, t + nudge);

            continue;
        }

        const float2 edge = float2(column) + select(directionAcross > 0.0, 1.0, 0.0);
        const float2 edgeT = select(moves, (edge - originAcross) / directionAcross, (float2)GLIMMER_BRICK_MISS_DISTANCE);
        const float tColumn = min(min(edgeT.x, edgeT.y), tOut);

        uint first;
        const uint count = GlimmerLoadBrickColumn(brick, column, first);

        const float3 middle = cellLow + GlimmerBrickPoint(axis, (float2(column) + 0.5) * width, 0.0);

        for (uint k = 0; k < count; k++)
        {
            const uint words = glimmerBrickCrossings[2u * (first + k)];
            const float depth = GlimmerBrickUnpackDepth(bricks, words & 0xFFFFu);
            const float3 normal = GlimmerBrickUnpackNormal(words >> 16u);
            const float facing = dot(normal, direction);

            if (abs(facing) < 1e-6)
            {
                continue;
            }

            const float3 onPlane = middle + GlimmerBrickPoint(axis, (float2)0.0, depth);
            const float tHit = dot(normal, onPlane - origin) / facing;

            if (tHit < t - slack || tHit > tColumn + slack || tHit <= tMin || tHit >= hit.t)
            {
                continue;
            }

            const uint2 part = min(uint2(saturate(originAcross + directionAcross * tHit - float2(column)) * 4.0), uint2(3u, 3u));
            const uint mask = glimmerBrickCrossings[2u * (first + k) + 1u] & 0xFFFFu;

            if (((mask >> (part.y * 4u + part.x)) & 1u) == 0u)
            {
                continue;
            }

            hit.t = tHit;
            hit.crossing = first + k;
            hit.brick = brick;
            hit.axis = axis;
            hit.normal = normal;
            hit.depth = depth;
        }

        t = max(tColumn, t + nudge);
    }
}

// direction is of unit length
bool TraceGlimmerBricks(
    GlimmerBrickParams bricks,
    float3 origin,
    float3 direction,
    float tMin,
    float tMax,
    out GlimmerBrickHit hit)
{
    hit.t = tMax;
    hit.crossing = GLIMMER_BRICK_NONE;
    hit.brick = GLIMMER_BRICK_NONE;
    hit.axis = 0u;
    hit.normal = -direction;
    hit.depth = 0.0;
    hit.steps = 0u;

    if (bricks.origin.w == 0)
    {
        return false;
    }

    const float size = bricks.params.x;
    const float2 range = GlimmerBrickWindowRange(bricks, origin, direction);

    float t = max(range.x, tMin);
    const float tEnd = min(range.y, tMax);

    if (t >= tEnd)
    {
        return false;
    }

    hit.t = tEnd;

    const float3 safeDirection = GlimmerBrickSafeDirection(direction);
    const bool3 moves = abs(direction) > 1e-12;
    const float nudge = 1e-4 * size / max(max(abs(direction.x), max(abs(direction.y), abs(direction.z))), 1e-3);

    const int3 cellMin = bricks.origin.xyz;
    const int3 cellMax = bricks.origin.xyz + GlimmerBrickGridSize - 1;

    const int maxSteps = GLIMMER_BRICK_GRID_XZ * 2 + GLIMMER_BRICK_GRID_Y + 3;

    [loop]
    for (int step = 0; step < maxSteps && t < tEnd && t < hit.t; step++)
    {
        hit.steps++;

        const float3 at = origin + direction * (t + nudge);
        const int3 cell = clamp(int3(floor(at * bricks.params.y)), cellMin, cellMax);

        const float3 cellLow = GlimmerBrickCellLow(bricks, cell);
        const float3 edge = cellLow + select(direction > 0.0, size, 0.0);
        const float3 edgeT = select(moves, (edge - origin) / safeDirection, (float3)GLIMMER_BRICK_MISS_DISTANCE);
        const float tCell = min(min(edgeT.x, min(edgeT.y, edgeT.z)), tEnd);

        const uint tableIndex = GlimmerBrickTableIndex(cell, 0u);

        [unroll]
        for (uint axis = 0; axis < 3u; axis++)
        {
            const uint brick = glimmerBrickTable[tableIndex + axis];

            if (brick != GLIMMER_BRICK_NONE)
            {
                GlimmerWalkBrick(bricks, brick, axis, cellLow, origin, direction, t, tCell, tMin, hit);
            }
        }

        t = max(tCell, t + nudge);
    }

    if (hit.crossing == GLIMMER_BRICK_NONE)
    {
        hit.t = tMax;

        return false;
    }

    return true;
}

struct GlimmerBrickReceiver
{
    bool found;
    uint crossing;
    uint brick;
    float3 normal;
    int2 column;
};

// the crossing a point on a surface is on: in the brick of the axis the normal is nearest, failing that of the next
GlimmerBrickReceiver GlimmerFindBrickReceiver(GlimmerBrickParams bricks, float3 position, float3 normal)
{
    GlimmerBrickReceiver receiver = (GlimmerBrickReceiver)0;

    const int3 cell = int3(floor(position * bricks.params.y));

    if (!GlimmerBrickCellInWindow(bricks, cell))
    {
        return receiver;
    }

    const float width = bricks.params.z;
    const float3 local = position - GlimmerBrickCellLow(bricks, cell);

    const uint firstAxis = GlimmerBrickAxis(normal);

    float3 rest = abs(normal);
    rest = select(firstAxis == 0u, float3(-1.0, rest.y, rest.z), select(firstAxis == 1u, float3(rest.x, -1.0, rest.z), float3(rest.x, rest.y, -1.0)));

    const uint axes[2] = { firstAxis, GlimmerBrickAxis(rest) };

    float nearest = 0.5 * width;

    for (uint axisIndex = 0; axisIndex < 2u && !receiver.found; axisIndex++)
    {
        const uint axis = axes[axisIndex];
        const uint brick = glimmerBrickTable[GlimmerBrickTableIndex(cell, axis)];

        if (brick == GLIMMER_BRICK_NONE)
        {
            continue;
        }

        const float2 across = GlimmerBrickAcross(axis, local) * bricks.params.w;
        const int2 column = clamp(int2(floor(across)), int2(0, 0), int2(GLIMMER_BRICK_SIDE - 1, GLIMMER_BRICK_SIDE - 1));
        const float2 offset = (across - (float2(column) + 0.5)) * width;

        uint first;
        const uint count = GlimmerLoadBrickColumn(brick, column, first);

        for (uint k = 0; k < count; k++)
        {
            const GlimmerBrickCrossing crossing = GlimmerLoadBrickCrossing(bricks, first + k);

            if (dot(crossing.normal, normal) < 0.5)
            {
                continue;
            }

            // the plane's depth where the point is, not at the middle of the column
            const float2 slope = GlimmerBrickAcross(axis, crossing.normal) / GlimmerBrickAlong(axis, crossing.normal);
            const float apart = abs(crossing.depth - dot(slope, offset) - GlimmerBrickAlong(axis, local));

            if (apart < nearest)
            {
                nearest = apart;

                receiver.found = true;
                receiver.crossing = first + k;
                receiver.brick = brick;
                receiver.normal = crossing.normal;
                receiver.column = column;
            }
        }
    }

    return receiver;
}

#endif

#ifdef GLIMMER_BRICKS_WITH_LIGHT

// the light a card sends on over a patch footprint metres across: rgb = radiance times how much of the patch is surface, a = that share. expects glimmerBrickRadiance
// read across the same card of the cells beside as well, so that neither a texel's edge nor a card's shows
// a surface sloping across a brick spreads over the cards of the layers its plane passes through: with its normal, every one of them is read; a zero normal reads the card alone
float4 GlimmerSampleBrickLight(GlimmerBrickParams bricks, int3 cell, uint axis, uint cardSlot, float3 position, float3 normal, float footprint)
{
    const float width = bricks.params.z;

    const uint level = uint(clamp(round(log2(max(footprint * bricks.params.w, 1.0))), float(GLIMMER_BRICK_CARD_FIRST_MIP), float(GLIMMER_BRICK_MIPS - 1)));
    const int side = int(uint(GLIMMER_BRICK_SIDE) >> level);

    const float2 across = GlimmerBrickAcross(axis, position - GlimmerBrickCellLow(bricks, cell)) * bricks.params.w;
    const float2 at = across / float(1u << level) - 0.5;
    const int2 corner = int2(floor(at));
    const float2 fraction = at - float2(corner);

    const uint mipOffset = GlimmerBrickCardMipOffset(level);

    // layers counted along the axis from the world's origin, four to a cell
    const uint facing = cardSlot / uint(GLIMMER_BRICK_LAYERS);
    const int cellAlong = int(GlimmerBrickAlong(axis, float3(cell)));
    const int ownLayer = cellAlong * int(GLIMMER_BRICK_LAYERS) + int(cardSlot % uint(GLIMMER_BRICK_LAYERS));
    const float layerSize = bricks.params.x / float(GLIMMER_BRICK_LAYERS);
    const float texelSize = width * float(1u << level);

    const bool hasPlane = dot(normal, normal) > 0.0;
    const float normalAlong = GlimmerBrickAlong(axis, normal);
    const float2 slope = hasPlane ? GlimmerBrickAcross(axis, normal) / max(abs(normalAlong), 0.5) * sign(normalAlong) : (float2)0.0;
    const float rise = (abs(slope.x) + abs(slope.y)) * 0.5 * texelSize;
    const float depth = GlimmerBrickAlong(axis, position);

    float4 sum = (float4)0.0;

    [unroll]
    for (uint tap = 0; tap < 4u; tap++)
    {
        const int2 offset = int2(tap & 1u, tap >> 1u);
        const int2 texel = corner + offset;
        const int2 beside = int2(floor(float2(texel) / float(side)));
        const int2 within = texel - beside * side;
        const float2 weights = lerp(1.0 - fraction, fraction, float2(offset));

        // the layers the plane is in across the texel, at most three, besides the card's own
        int low = ownLayer;
        int high = ownLayer - 1;

        if (hasPlane)
        {
            const float2 middle = (float2(texel) + 0.5) * texelSize - across * width;
            const float there = depth - dot(slope, middle);

            low = int(floor((there - rise) / layerSize));
            high = min(int(floor((there + rise) / layerSize)), low + 2);
        }

        for (int k = -1; k <= high - low; k++)
        {
            const int layer = k < 0 ? ownLayer : low + k;

            if (k >= 0 && layer == ownLayer)
            {
                continue;
            }

            const int layerCell = int(floor(float(layer) / float(GLIMMER_BRICK_LAYERS)));
            const int3 otherCell = cell + int3(GlimmerBrickPoint(axis, float2(beside), float(layerCell - cellAlong)));

            if (!GlimmerBrickCellInWindow(bricks, otherCell))
            {
                continue;
            }

            const uint brick = glimmerBrickTable[GlimmerBrickTableIndex(otherCell, axis)];

            if (brick == GLIMMER_BRICK_NONE)
            {
                continue;
            }

            const uint slot = facing * uint(GLIMMER_BRICK_LAYERS) + uint(layer - layerCell * int(GLIMMER_BRICK_LAYERS));
            const uint card = glimmerBrickIndex[GlimmerBrickWord(brick, uint(GLIMMER_BRICK_INDEX_CARDS) + slot)];

            if (card == GLIMMER_BRICK_NONE)
            {
                continue;
            }

            sum += GlimmerBrickUnpackLight(glimmerBrickRadiance[card * uint(GLIMMER_BRICK_CARD_TEXELS) + mipOffset + uint(within.y * side + within.x)]) * (weights.x * weights.y);
        }
    }

    return sum;
}

#endif

#endif
