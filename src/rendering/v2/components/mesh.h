//
// Created by emd22 on 2022-03-19.
//

#ifndef HYPERION_MESH_H
#define HYPERION_MESH_H

#include <cstdint>
#include <vector>

#include <rendering/backend/renderer_buffer.h>
#include <math/vertex.h>

namespace hyperion::v2 {

class Mesh {
    using Index = uint32_t;
public:

private:
    renderer::GPUBuffer *vbo = nullptr;
    renderer::GPUBuffer *ibo = nullptr;

    std::vector<Vertex> vertices;
    std::vector<Index>  indices;
};

}

#endif //HYPERION_MESH_H
