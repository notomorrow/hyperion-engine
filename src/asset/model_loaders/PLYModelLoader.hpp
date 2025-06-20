/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PLY_MODEL_LOADER_HPP
#define HYPERION_PLY_MODEL_LOADER_HPP

#include <asset/Assets.hpp>
#include <scene/Node.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class PLYModelLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(PLYModelLoader);
    
public:
    enum PLYType
    {
        PLY_TYPE_UNKNOWN = -1,
        PLY_TYPE_DOUBLE,
        PLY_TYPE_FLOAT,
        PLY_TYPE_INT,
        PLY_TYPE_UINT,
        PLY_TYPE_SHORT,
        PLY_TYPE_USHORT,
        PLY_TYPE_CHAR,
        PLY_TYPE_UCHAR,
        PLY_TYPE_MAX
    };

    struct PLYPropertyDefinition
    {
        PLYType type;
        SizeType offset;
    };

    struct PLYModel
    {
        HashMap<String, PLYPropertyDefinition> property_types;
        HashMap<String, ByteBuffer> custom_data;
        Array<Vertex> vertices;
        SizeType header_length = 0;
    };

    static PLYType StringToPLYType(const String& str);

    static SizeType PLYTypeSize(PLYType);

    virtual ~PLYModelLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override
    {
        PLYModel model = LoadModel(state);

        return BuildModel(state, model);
    }

    static PLYModel LoadModel(LoaderState& state);
    static LoadedAsset BuildModel(LoaderState& state, PLYModel& model);
};

} // namespace hyperion

#endif