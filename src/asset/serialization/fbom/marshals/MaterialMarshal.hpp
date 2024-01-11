#ifndef HYPERION_V2_FBOM_MARSHALS_MATERIAL_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_MATERIAL_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/TextureMarshal.hpp>
#include <rendering/Material.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Material> : public FBOMObjectMarshalerBase<Material>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType(Material::GetClass().GetName());
    }

    virtual FBOMResult Serialize(const Material &in_object, FBOMObject &out) const override
    {
        out.SetProperty("name", FBOMName(), in_object.GetName());

        out.SetProperty("attributes.bucket", FBOMUnsignedInt(), UInt32(in_object.GetRenderAttributes().bucket));
        out.SetProperty("attributes.flags", FBOMUnsignedInt(), UInt32(in_object.GetRenderAttributes().flags));
        out.SetProperty("attributes.cull_mode", FBOMUnsignedInt(), UInt32(in_object.GetRenderAttributes().cull_faces));
        out.SetProperty("attributes.fill_mode", FBOMUnsignedInt(), UInt32(in_object.GetRenderAttributes().fill_mode));

        out.SetProperty("params.size", FBOMUnsignedInt(), UInt32(in_object.GetParameters().Size()));

        for (const auto &it : in_object.GetParameters()) {
            const auto key = it.first;
            const auto &value = it.second;

            out.SetProperty(
                String("params.") + String::ToString(UInt(key)) + ".key",
                FBOMUnsignedLong(),
                key
            );

            out.SetProperty(
                String("params.") + String::ToString(UInt(key)) + ".type",
                FBOMUnsignedInt(),
                value.type
            );

            if (value.IsIntType()) {
                for (UInt j = 0; j < 4; j++) {
                    out.SetProperty(
                        String("params.") + String::ToString(UInt(key)) + ".values[" + String::ToString(j) + "]",
                        FBOMInt(),
                        value.values.int_values[j]
                    );
                }
            } else if (value.IsFloatType()) {
                for (UInt j = 0; j < 4; j++) {
                    out.SetProperty(
                        String("params.") + String::ToString(UInt(key)) + ".values[" + String::ToString(j) + "]",
                        FBOMFloat(),
                        value.values.float_values[j]
                    );
                }
            }
        }

        UInt32 texture_keys[Material::max_textures];
        Memory::MemSet(&texture_keys[0], 0, sizeof(texture_keys));

        UInt texture_index = 0;

        for (const auto &it : in_object.GetTextures()) {
            const auto key = it.first;
            const auto &value = it.second;

            if (value) {
                if (auto err = out.AddChild(*value, FBOM_OBJECT_FLAGS_EXTERNAL)) {
                    return err;
                }

                texture_keys[texture_index++] = UInt32(key);
            }
        }

        if (const auto &shader = in_object.GetShader()) {
            out.AddChild(*shader, FBOM_OBJECT_FLAGS_EXTERNAL);
        }

        out.SetProperty(
            "texture_keys",
            FBOMArray(FBOMUnsignedInt(), std::size(texture_keys)),
            &texture_keys[0]
        );

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        Name name;
        in.GetProperty("name").ReadName(&name);

        MaterialAttributes attributes;
        Material::ParameterTable parameters = Material::DefaultParameters();
        Material::TextureSet textures;

        in.GetProperty("attributes.bucket").ReadUInt32(&attributes.bucket);
        in.GetProperty("attributes.flags").ReadUInt32(&attributes.flags);
        in.GetProperty("attributes.cull_mode").ReadUInt32(&attributes.cull_faces);
        in.GetProperty("attributes.fill_mode").ReadUInt32(&attributes.fill_mode);

        UInt32 num_parameters;

        // load parameters
        if (auto err = in.GetProperty("params.size").ReadUInt32(&num_parameters)) {
            return err;
        }

        for (UInt i = 0; i < num_parameters; i++) {
            const auto param_string = String("params.") + String::ToString(i);

            Material::MaterialKey key;
            Material::Parameter param;

            if (auto err = in.GetProperty(param_string).ReadArrayElements(FBOMFloat(), 4, &param.values)) {
                continue;
            }

            if (auto err = in.GetProperty(param_string + ".key").ReadUInt64(&key)) {
                continue;
            }

            if (auto err = in.GetProperty(param_string + ".type").ReadUInt32(&param.type)) {
                continue;
            }

            if (param.IsIntType()) {
                for (UInt j = 0; j < 4; j++) {
                    in.GetProperty(param_string + ".values[" + String::ToString(j) + "]")
                        .ReadInt32(&param.values.int_values[j]);
                }
            } else if (param.IsFloatType()) {
                for (UInt j = 0; j < 4; j++) {
                    in.GetProperty(param_string + ".values[" + String::ToString(j) + "]")
                        .ReadFloat(&param.values.float_values[j]);
                }
            }

            parameters.Set(key, param);
        }

        UInt32 texture_keys[Material::max_textures];
        Memory::MemSet(&texture_keys[0], 0, sizeof(texture_keys));

        if (auto err = in.GetProperty("texture_keys").ReadArrayElements(FBOMUnsignedInt(), std::size(texture_keys), &texture_keys[0])) {
            return err;
        }

        UInt texture_index = 0;

        Handle<Shader> shader = g_shader_manager->GetOrCreate(
            HYP_NAME(Forward),
            ShaderProperties()
        );

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("Texture")) {
                if (texture_index < std::size(texture_keys)) {
                    if (auto texture = node.deserialized.Get<Texture>()) {
                        textures.Set(
                            Material::TextureKey(texture_keys[texture_index]), 
                            std::move(texture)
                        );

                        ++texture_index;
                    }
                }
            } else if (node.GetType().IsOrExtends("Shader")) {
                shader = node.deserialized.Get<Shader>();
            }
        }

        auto material_handle = g_material_system->GetOrCreate(attributes, parameters, textures);
        material_handle->SetShader(std::move(shader));

        if (name) {
            material_handle->SetName(name);
        }

        out_object = std::move(UniquePtr<Handle<Material>>::Construct(material_handle));

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif