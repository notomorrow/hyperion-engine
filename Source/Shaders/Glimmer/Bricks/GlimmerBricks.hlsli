#ifndef GLIMMER_BRICKS_HLSLI
#define GLIMMER_BRICKS_HLSLI

// static geometry as surface crossings along columns: up to three bricks a cell, one per axis, a triangle in the one its normal is nearest
// expects glimmerBrickTable, glimmerBrickIndex and glimmerBrickCrossings to be declared

// NOTE must match GlimmerBrickWorld.hpp
#define GLIMMER_BRICK_GRID_XZ 64
#define GLIMMER_BRICK_GRID_Y 32
#define GLIMMER_BRICK_CHUNK_CELLS 8
#define GLIMMER_BRICK_SIDE 32
#define GLIMMER_BRICK_TILE 4
#define GLIMMER_BRICK_TILES_PER_SIDE (GLIMMER_BRICK_SIDE / GLIMMER_BRICK_TILE)
#define GLIMMER_BRICK_MAX_COLUMN_CROSSINGS 8
#define GLIMMER_BRICK_PAGE_CROSSINGS 256
#define GLIMMER_BRICK_MAX_PAGES 31

// per brick in glimmerBrickIndex: its pages of crossings (count first), then a depth range per tile, then first crossing and count per column
#define GLIMMER_BRICK_INDEX_TILES (1 + GLIMMER_BRICK_MAX_PAGES)
#define GLIMMER_BRICK_INDEX_COLUMNS (GLIMMER_BRICK_INDEX_TILES + GLIMMER_BRICK_TILES_PER_SIDE * GLIMMER_BRICK_TILES_PER_SIDE)
#define GLIMMER_BRICK_INDEX_STRIDE (GLIMMER_BRICK_INDEX_COLUMNS + GLIMMER_BRICK_SIDE * GLIMMER_BRICK_SIDE)

#define GLIMMER_BRICK_NONE 0xFFFFFFFFu
#define GLIMMER_BRICK_MISS_DISTANCE 3.402823466e+38f

#define GLIMMER_BRICK_FLAG_BURIED 1u
#define GLIMMER_BRICK_FLAG_CURVED 2u
#define GLIMMER_BRICK_FLAG_DOUBLE_SIDED 4u

// columns beyond its own that a crossing's plane still stops a ray
#define GLIMMER_BRICK_SLACK 0.25

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

// x before y before z where they tie
uint GlimmerBrickAxis(float3 normal)
{
    const float3 a = abs(normal);

    return a.x >= a.y && a.x >= a.z ? 0u : (a.y >= a.z ? 1u : 2u);
}

// octahedral, the six axes kept exactly
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

uint GlimmerBrickU(uint axis)
{
    return (axis + 1u) % 3u;
}

uint GlimmerBrickV(uint axis)
{
    return (axis + 2u) % 3u;
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

uint GlimmerBrickTileIndex(uint brick, int2 tile)
{
    return brick * uint(GLIMMER_BRICK_INDEX_STRIDE) + uint(GLIMMER_BRICK_INDEX_TILES) + uint(tile.y) * uint(GLIMMER_BRICK_TILES_PER_SIDE) + uint(tile.x);
}

uint GlimmerBrickColumnIndex(uint brick, int2 column)
{
    return brick * uint(GLIMMER_BRICK_INDEX_STRIDE) + uint(GLIMMER_BRICK_INDEX_COLUMNS) + uint(column.y) * uint(GLIMMER_BRICK_SIDE) + uint(column.x);
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

#ifndef GLIMMER_BRICKS_NO_TRACE

struct GlimmerBrickCrossing
{
    float depth; // from the cell's low side, along the brick's axis, at the middle of the column
    float3 normal;
    uint mask;   // a bit per sixteenth of the column the surface is in
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

        const uint entry = glimmerBrickIndex[GlimmerBrickColumnIndex(brick, column)];
        const uint first = entry & 0xFFFFFFu;
        const uint count = entry >> 24u;

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
bool TraceGlimmerBricks(GlimmerBrickParams bricks, float3 origin, float3 direction, float tMin, float tMax, out GlimmerBrickHit hit)
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

    const float3 low = float3(bricks.origin.xyz) * size;
    const float3 high = low + float3(GlimmerBrickGridSize) * size;

    const float3 safeDirection = select(abs(direction) > 1e-12, direction, select(direction >= 0.0, 1e-12, -1e-12));
    const float3 inverseDirection = 1.0 / safeDirection;

    const float3 t0 = (low - origin) * inverseDirection;
    const float3 t1 = (high - origin) * inverseDirection;
    const float3 tNear = min(t0, t1);
    const float3 tFar = max(t0, t1);

    float t = max(max(tNear.x, max(tNear.y, tNear.z)), tMin);
    const float tEnd = min(min(tFar.x, min(tFar.y, tFar.z)), tMax);

    if (t >= tEnd)
    {
        return false;
    }

    hit.t = tEnd;

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
    uint axis;
    float3 normal;
    float depth;
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

        const uint entry = glimmerBrickIndex[GlimmerBrickColumnIndex(brick, column)];
        const uint first = entry & 0xFFFFFFu;
        const uint count = entry >> 24u;

        for (uint k = 0; k < count; k++)
        {
            const uint words = glimmerBrickCrossings[2u * (first + k)];
            const float depth = GlimmerBrickUnpackDepth(bricks, words & 0xFFFFu);
            const float3 crossingNormal = GlimmerBrickUnpackNormal(words >> 16u);

            if (dot(crossingNormal, normal) < 0.5)
            {
                continue;
            }

            // the plane's depth where the point is, not at the middle of the column
            const float2 slope = GlimmerBrickAcross(axis, crossingNormal) / GlimmerBrickAlong(axis, crossingNormal);
            const float apart = abs(depth - dot(slope, offset) - GlimmerBrickAlong(axis, local));

            if (apart < nearest)
            {
                nearest = apart;

                receiver.found = true;
                receiver.crossing = first + k;
                receiver.brick = brick;
                receiver.axis = axis;
                receiver.normal = crossingNormal;
                receiver.depth = depth;
                receiver.column = column;
            }
        }
    }

    return receiver;
}

#endif

#endif
