#ifndef MESH_ARRAY_H
#define MESH_ARRAY_H

#include "../mesh.h"
#include "../material.h"
#include "../../math/transform.h"

#include <vector>
#include <memory>

namespace apex {
struct Submesh {
    std::shared_ptr<Mesh> mesh;
    Transform transform;
};

class MeshArray : public Renderable {
public:
    MeshArray();
    virtual ~MeshArray() = default;

    virtual void Render() override;

    // merges all submeshes into one
    void Optimize();

protected:
    std::vector<Submesh> m_submeshes;

    void UpdateSubmeshes();
    void ApplyTransforms();
};
} // namespace apex

#endif
