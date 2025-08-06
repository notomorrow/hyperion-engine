/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/Assets.hpp>

#include <scene/Node.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>
#include <core/utilities/Tuple.hpp>

#include <core/Types.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class OBJModelLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(OBJModelLoader);

public:
    struct OBJModel
    {
        struct OBJIndex
        {
            int64 vertex,
                normal,
                texcoord;

            HYP_FORCE_INLINE bool operator==(const OBJIndex& other) const
            {
                return vertex == other.vertex
                    && normal == other.normal
                    && texcoord == other.texcoord;
            }

            HYP_FORCE_INLINE bool operator<(const OBJIndex& other) const
            {
                return Tie(vertex, normal, texcoord) < Tie(other.vertex, other.normal, other.texcoord);
            }

            HYP_FORCE_INLINE HashCode GetHashCode() const
            {
                HashCode hc;
                hc.Add(vertex);
                hc.Add(normal);
                hc.Add(texcoord);

                return hc;
            }
        };

        struct OBJMesh
        {
            String name;
            String material;
            Array<OBJIndex> indices;
        };

        String filepath;

        Array<Vec3f> positions;
        Array<Vec3f> normals;
        Array<Vec2f> texcoords;
        Array<OBJMesh> meshes;
        String name;
        String materialLibrary;
    };

    virtual ~OBJModelLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override
    {
        OBJModel model = LoadModel(state);

        return BuildModel(state, model);
    }

    static OBJModel LoadModel(LoaderState& state);
    static LoadedAsset BuildModel(LoaderState& state, OBJModel& model);
};

using OBJIndex = OBJModelLoader::OBJModel::OBJIndex;

} // namespace hyperion

namespace std {
template <>
struct hash<hyperion::OBJIndex>
{
    size_t operator()(const hyperion::OBJIndex& obj) const
    {
        hyperion::HashCode hc;
        hc.Add(obj.vertex);
        hc.Add(obj.normal);
        hc.Add(obj.texcoord);

        return hc.Value();
    }
};
} // namespace std

