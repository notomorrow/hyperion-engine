#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "../asset_loader.h"
#include "../../math/vector2.h"
#include "../../math/vector3.h"

#include <vector>
#include <string>
#include <cstring>
#include <map>
#include <tuple>

namespace hyperion {
class MtlLib;

struct ObjIndex {
    int vertex_idx, normal_idx, texcoord_idx;

    inline bool operator==(const ObjIndex &other) const
        { return std::memcmp(this, &other, sizeof(ObjIndex)) == 0; }

    inline bool operator<(const ObjIndex &other) const
        { return std::tie(vertex_idx, normal_idx, texcoord_idx) < std::tie(other.vertex_idx, other.normal_idx, other.texcoord_idx); }
};

struct ObjMesh {
    std::string name;
    std::string mtl;
    std::vector<ObjIndex> indices;
};

struct ObjModel {
    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::vector<ObjMesh> meshes;
    std::shared_ptr<MtlLib> mtl_lib;

    bool has_normals, has_texcoords;

    ObjModel()
        : has_normals(false),
          has_texcoords(false)
    {
    }

    void AddMesh(const std::string &name);
    ObjMesh &CurrentList();
    ObjIndex ParseObjIndex(const std::string &);
};

class ObjLoader : public AssetLoader {
public:
    static const bool use_indices;

    virtual ~ObjLoader() = default;

    std::shared_ptr<Loadable> LoadFromFile(const std::string &);
};
}

#endif
