/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMArray.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/object/HypData.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <scene/Material.hpp>
#include <scene/Texture.hpp>

#include <rendering/backend/RendererShader.hpp>

#include <HyperionEngine.hpp>

namespace hyperion::serialization {

template <>
class FBOMMarshaler<Material> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out))
        {
            return err;
        }

        const Material& in_object = in.Get<Material>();

        out.SetProperty("Attributes", FBOMData::FromObject(FBOMObject().SetProperty("Bucket", uint32(in_object.GetRenderAttributes().bucket)).SetProperty("Flags", uint32(in_object.GetRenderAttributes().flags)).SetProperty("CullMode", uint32(in_object.GetRenderAttributes().cull_faces)).SetProperty("FillMode", uint32(in_object.GetRenderAttributes().fill_mode))));

        FBOMArray params_array { FBOMBaseObjectType() };

        for (SizeType i = 0; i < in_object.GetParameters().Size(); i++)
        {
            const auto key_value = in_object.GetParameters().KeyValueAt(i);

            FBOMObject param_object;
            param_object.SetProperty("Key", uint64(key_value.first));
            param_object.SetProperty("Type", uint32(key_value.second.type));

            if (key_value.second.IsIntType())
            {
                param_object.SetProperty("Data", FBOMSequence(FBOMInt32(), ArraySize(key_value.second.values.int_values)), ArraySize(key_value.second.values.int_values) * sizeof(int32), &key_value.second.values.int_values[0]);
            }
            else if (key_value.second.IsFloatType())
            {
                param_object.SetProperty("Data", FBOMSequence(FBOMFloat(), ArraySize(key_value.second.values.float_values)), ArraySize(key_value.second.values.int_values) * sizeof(float), &key_value.second.values.float_values[0]);
            }

            params_array.AddElement(FBOMData::FromObject(std::move(param_object)));
        }

        out.SetProperty("Parameters", FBOMData::FromArray(std::move(params_array)));

        FixedArray<uint32, Material::max_textures> texture_keys = { 0 };

        // for (SizeType i = 0, texture_index = 0; i < in_object.GetTextures().Size(); i++) {
        //     if (texture_index >= texture_keys.Size()) {
        //         break;
        //     }

        //     const MaterialTextureKey key = in_object.GetTextures().KeyAt(i);
        //     const Handle<Texture> &value = in_object.GetTextures().ValueAt(i);

        //     if (value.IsValid()) {
        //         if (FBOMResult err = out.AddChild(*value, FBOMObjectSerializeFlags::EXTERNAL)) {
        //             return err;
        //         }

        //         texture_keys[texture_index++] = uint32(key);
        //     }
        // }

        out.SetProperty(
            "TextureKeys",
            FBOMSequence(FBOMUInt32(), texture_keys.Size()),
            texture_keys.ByteSize(),
            &texture_keys[0]);

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        MaterialAttributes attributes;
        Material::ParameterTable parameters = Material::DefaultParameters();
        Material::TextureSet textures;

        FBOMObject attributes_object;

        if (FBOMResult err = in.GetProperty("Attributes").ReadObject(context, attributes_object))
        {
            return err;
        }

        attributes_object.GetProperty("Bucket").ReadUInt32(&attributes.bucket);
        attributes_object.GetProperty("Flags").ReadUInt32(&attributes.flags);
        attributes_object.GetProperty("CullMode").ReadUInt32(&attributes.cull_faces);
        attributes_object.GetProperty("FillMode").ReadUInt32(&attributes.fill_mode);

        FBOMArray params_array { FBOMUnset() };

        if (FBOMResult err = in.GetProperty("Parameters").ReadArray(context, params_array))
        {
            return err;
        }

        for (SizeType i = 0; i < params_array.Size(); i++)
        {
            Material::Parameter param;

            if (const FBOMData& element = params_array.GetElement(i))
            {
                FBOMObject param_object;

                if (FBOMResult err = element.ReadObject(context, param_object))
                {
                    return err;
                }

                Material::MaterialKey param_key;

                if (FBOMResult err = param_object.GetProperty("Key").ReadUInt64(&param_key))
                {
                    return err;
                }

                if (FBOMResult err = param_object.GetProperty("Type").ReadUInt32(&param.type))
                {
                    return err;
                }

                if (param.IsIntType())
                {
                    if (FBOMResult err = param_object.GetProperty("Data").ReadElements(FBOMInt32(), ArraySize(param.values.int_values), &param.values.int_values[0]))
                    {
                        return err;
                    }
                }
                else if (param.IsFloatType())
                {
                    if (FBOMResult err = param_object.GetProperty("Data").ReadElements(FBOMFloat(), ArraySize(param.values.float_values), &param.values.float_values[0]))
                    {
                        return err;
                    }
                }

                parameters.Set(param_key, param);
            }
        }

        FixedArray<uint32, Material::max_textures> texture_keys { 0 };

        if (FBOMResult err = in.GetProperty("TextureKeys").ReadElements(FBOMUInt32(), texture_keys.Size(), &texture_keys[0]))
        {
            return err;
        }

        uint32 texture_index = 0;

        ShaderRef shader = g_shader_manager->GetOrCreate(
            NAME("Forward"),
            ShaderProperties(static_mesh_vertex_attributes));

        attributes.shader_definition = shader->GetCompiledShader()->GetDefinition();

        for (const FBOMObject& child : in.GetChildren())
        {
            HYP_LOG(Serialization, Debug, "Material : Child TypeID: {}, TypeName: {}", child.GetType().GetNativeTypeID().Value(), child.GetType().name);
            if (child.GetType().IsOrExtends("Texture"))
            {
                if (texture_index < texture_keys.Size())
                {
                    if (Optional<const Handle<Texture>&> texture_opt = child.m_deserialized_object->TryGet<Handle<Texture>>())
                    {
                        textures.Set(
                            MaterialTextureKey(texture_keys[texture_index]),
                            *texture_opt);

                        ++texture_index;
                    }
                }
            }
        }

        Handle<Material> material_handle = g_material_system->GetOrCreate(attributes, parameters, textures);

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(context, in, Material::Class(), AnyRef(*material_handle)))
        {
            return err;
        }

        out = HypData(std::move(material_handle));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Material, FBOMMarshaler<Material>);

} // namespace hyperion::serialization