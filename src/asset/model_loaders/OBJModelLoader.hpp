#ifndef HYPERION_V2_OBJ_MODEL_LOADER_H
#define HYPERION_V2_OBJ_MODEL_LOADER_H

#include <asset/Assets.hpp>
#include <scene/Node.hpp>
#include <core/Containers.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class OBJModelLoader : public AssetLoader
{
public:
    struct OBJModel
    {
        struct OBJIndex
        {
            Int64 vertex,
                normal,
                texcoord;

            bool operator<(const OBJIndex &other) const
                { return std::tie(vertex, normal, texcoord) < std::tie(other.vertex, other.normal, other.texcoord); }
        };

        struct OBJMesh
        {
            std::string tag;
            std::string material;
            std::vector<OBJIndex> indices;
        };

        std::string filepath;

        std::vector<Vector3> positions;
        std::vector<Vector3> normals;
        std::vector<Vector2> texcoords;
        std::vector<OBJMesh> meshes;
        std::string tag;
        std::string material_library;
    };

    virtual ~OBJModelLoader() = default;

    virtual LoadAssetResultPair LoadAsset(LoaderState &state) const override
    {
        OBJModel model = LoadModel(state);

        return BuildModel(state, model);
    }
    
    static OBJModel LoadModel(LoaderState &state);
    static LoadAssetResultPair BuildModel(LoaderState &state, OBJModel &model);
};

using OBJIndex = OBJModelLoader::OBJModel::OBJIndex;

} // namespace hyperion::v2

namespace std {
template<>
struct hash<hyperion::v2::OBJIndex> {
    size_t operator()(const hyperion::v2::OBJIndex &obj) const
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