#include "renderer_structs.h"

#include <utility>

namespace hyperion::renderer {

const decltype(MeshInputAttribute::mapping) MeshInputAttribute::mapping({
    std::make_pair(MESH_INPUT_ATTRIBUTE_POSITION,     MeshInputAttribute{.location = 0, .binding = 0, .size = 3 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_NORMAL,       MeshInputAttribute{.location = 1, .binding = 0, .size = 3 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_TEXCOORD0,    MeshInputAttribute{.location = 2, .binding = 0, .size = 2 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_TEXCOORD1,    MeshInputAttribute{.location = 3, .binding = 0, .size = 2 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_TANGENT,      MeshInputAttribute{.location = 4, .binding = 0, .size = 3 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_BITANGENT,    MeshInputAttribute{.location = 5, .binding = 0, .size = 3 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_BONE_INDICES, MeshInputAttribute{.location = 6, .binding = 0, .size = 4 * sizeof(float)}),
    std::make_pair(MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS, MeshInputAttribute{.location = 7, .binding = 0, .size = 4 * sizeof(float)})
});

} // hyperion::renderer