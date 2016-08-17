#ifndef MESH_FACTORY_H
#define MESH_FACTORY_H

#include "../rendering/mesh.h"

#include <memory>

namespace apex {
class MeshFactory {
public:
    static std::shared_ptr<Mesh> CreateQuad();
};
}

#endif