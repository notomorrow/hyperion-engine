#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "../asset_loader.h"
#include "../../math/vector2.h"
#include "../../math/vector3.h"

#include <vector>
#include <string>

namespace apex {
struct ObjModel {
    struct ObjIndex {
        int vertex_idx, normal_idx, texcoord_idx;
    };

    std::vector<std::string> mesh_names;
    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::vector<std::vector<ObjIndex>> indices;

    bool has_normals, has_texcoords;

    ObjModel()
        : has_normals(false), has_texcoords(false)
    {
    }

    void AddMesh(const std::string &name);
    std::vector<ObjIndex> &CurrentList();
    ObjIndex ParseObjIndex(const std::string &);
};

class ObjLoader : public AssetLoader {
public:
    std::shared_ptr<Loadable> LoadFromFile(const std::string &);
};
}

#endif