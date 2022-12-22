#ifndef HYPERION_V2_FBOM_MARSHALS_SHADER_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_SHADER_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/SubShaderMarshal.hpp>
#include <rendering/Shader.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Shader> : public FBOMObjectMarshalerBase<Shader>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType("ShaderInstance");
    }

    virtual FBOMResult Serialize(const Shader &in_object, FBOMObject &out) const override
    {
        //out.AddChild(in_object.GetCompiledShader(), FBOM_OBJECT_FLAGS_EXTERNAL);

        out.SetProperty("name", FBOMString(), in_object.GetCompiledShader().name.Size(), in_object.GetCompiledShader().name.Data());

        auto properties_array = in_object.GetCompiledShader().properties.ToArray();

        out.SetProperty("properties.size", FBOMUnsignedInt(), UInt(properties_array.Size()));

        for (SizeType index = 0; index < properties_array.Size(); index++) {
            const ShaderProperty &item = properties_array[index];

            out.SetProperty(
                String("properties.") + String::ToString(index) + ".name",
                FBOMString(),
                item.name.Size(),
                item.name.Data()
            );
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        auto shader = CreateObject<Shader>();

        String name;

        if (auto err = in.GetProperty("name").ReadString(name)) {
            return err;
        }

        ShaderProps properties;

        UInt num_properties;

        if (auto err = in.GetProperty("properties.size").ReadUnsignedInt(&num_properties)) {
            return err;
        }

        for (UInt i = 0; i < num_properties; i++) {
            const auto param_string = String("properties.") + String::ToString(i);

            String property_name;

            if (auto err = in.GetProperty(param_string + ".name").ReadString(property_name)) {
                continue;
            }

            properties.Set(property_name);
        }

        auto compiled_shader = Engine::Get()->GetShaderCompiler().GetCompiledShader(name, properties);

        if (!compiled_shader.IsValid()) {
            DebugLog(
                LogType::Error,
                "Failed to deserialize Shader instance: The referenced compiled shader is not valid.\n"
                "\tName: %s\n"
                "\tProperties: %s\n",
                name.Data(),
                properties.ToString().Data()
            );

            return { FBOMResult::FBOM_ERR, "Invalid compiled shader" };
        }

        shader->SetCompiledShader(std::move(compiled_shader));

        out_object = UniquePtr<Handle<Shader>>::Construct(std::move(shader));

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif