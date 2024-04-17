/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_OGRE_XML_MODEL_LOADER_HPP
#define HYPERION_OGRE_XML_MODEL_LOADER_HPP

#include <asset/Assets.hpp>
#include <scene/Node.hpp>
#include <core/Containers.hpp>
#include <rendering/Mesh.hpp>

#include <Types.hpp>

namespace hyperion {

class OgreXMLModelLoader : public AssetLoader
{
public:
    struct OgreXMLModel
    {
        struct SubMesh
        {
            Array<Mesh::Index> indices;
        };

        struct BoneAssignment
        {
            uint    index;
            float   weight;
        };

        String                                  filepath;

        Array<Vector3>                          positions;
        Array<Vector3>                          normals;
        Array<Vector2>                          texcoords;

        Array<Vertex>                           vertices;

        Array<SubMesh>                          submeshes;
        FlatMap<uint, Array<BoneAssignment>>    bone_assignments;

        String                                  skeleton_name;
    };

    virtual ~OgreXMLModelLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion

#endif