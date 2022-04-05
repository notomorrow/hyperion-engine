#ifndef HYPERION_V2_OBJ_MODEL_LOADER_H
#define HYPERION_V2_OBJ_MODEL_LOADER_H

#include <rendering/v2/asset/loader_object.h>
#include <rendering/v2/asset/loader.h>

namespace hyperion::v2 {

struct ObjNodeStub {};

template <>
struct LoaderObject<ObjNodeStub> {
    struct ObjIndex {
        size_t vertex,
               normal,
               texcoord;
    };

    struct ObjMesh {
        std::string           tag;
        std::string           material;
        std::vector<ObjIndex> indices;
    };

    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::vector<ObjMesh> meshes;
    std::string          material_library;
};

class ObjModelLoader : public Loader<Node, ObjNodeStub> {
    static LoaderResult LoadFn(LoaderStream *stream, Object &);
    static std::unique_ptr<Node> BuildFn(const Object &);

public:
    ObjModelLoader()
        : Loader({
            .load_fn = LoadFn,
            .build_fn = BuildFn
        })
    {
    }
};

} // namespace hyperion::v2

#endif