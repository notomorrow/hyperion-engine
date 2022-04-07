#ifndef HYPERION_V2_OBJ_MODEL_LOADER_H
#define HYPERION_V2_OBJ_MODEL_LOADER_H

#include <rendering/v2/asset/loader_object.h>
#include <rendering/v2/asset/loader.h>

namespace hyperion::v2 {

template <>
struct LoaderObject<Node, LoaderFormat::MODEL_OBJ> {
    class Loader : public LoaderBase<Node, LoaderFormat::MODEL_OBJ> {
        static LoaderResult LoadFn(LoaderStream *stream, Object &);
        static std::unique_ptr<Node> BuildFn(Engine *engine, const Object &);

    public:
        Loader()
            : LoaderBase({
                .load_fn = LoadFn,
                .build_fn = BuildFn
            })
        {
        }
    };

    struct ObjIndex {
        int64_t vertex,
                normal,
                texcoord;

        bool operator<(const ObjIndex &other) const
            { return std::tie(vertex, normal, texcoord) < std::tie(other.vertex, other.normal, other.texcoord); }
    };

    struct ObjMesh {
        std::string           tag;
        std::string           material;
        std::vector<ObjIndex>  indices;
    };

    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::vector<ObjMesh> meshes;
    std::string          material_library;
};

using ObjIndex = LoaderObject<Node, LoaderFormat::MODEL_OBJ>::ObjIndex;

} // namespace hyperion::v2

namespace std {
template<>
struct hash<hyperion::v2::ObjIndex> {
    inline size_t operator()(const hyperion::v2::ObjIndex &obj) const
    {
        hyperion::HashCode hc;
        hc.Add(obj.vertex);
        hc.Add(obj.normal);
        hc.Add(obj.texcoord);

        return hc.Value();
    }
};
} // namespace std

#endif