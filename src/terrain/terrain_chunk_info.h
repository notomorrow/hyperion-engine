#ifndef HYPERION_V2_TERRAIN_CHUNK_INFO_H
#define HYPERION_V2_TERRAIN_CHUNK_INFO_H

#include <math/vector2.h>
#include <math/vector3.h>
#include <rendering/backend/renderer_structs.h>

#include <types.h>

#include <memory>

namespace hyperion::v2 {

using renderer::Extent3D;

// struct TerrainChunkNeighborInfo {
//     Vector2 position;
//     bool    in_queue;
// };

// struct TerrainChunkInfo {
//     Extent3D extent;
//     Vector2  position;
//     Vector3  scale;
// };

} // namespace hyperion::v2

#endif