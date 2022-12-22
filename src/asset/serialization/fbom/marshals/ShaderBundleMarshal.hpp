#ifndef HYPERION_V2_FBOM_MARSHALS_SHADER_BUNDLE_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_SHADER_BUNDLE_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <util/shader_compiler/ShaderCompiler.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<CompiledShader> : public FBOMObjectMarshalerBase<CompiledShader>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType("CompiledShader");
    }

    virtual FBOMResult Serialize(const CompiledShader &in_object, FBOMObject &out) const override
    {
        if (!in_object.IsValid()) {
            return { FBOMResult::FBOM_ERR, "Cannot serialize invalid compiled shader instance" };
        }

        out.SetProperty("name", FBOMString(), in_object.name.Size(), in_object.name.Data());

        auto properties_array = in_object.properties.ToArray();

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

        for (SizeType index = 0; index < in_object.modules.Size(); index++) {
            const auto &byte_buffer = in_object.modules[index];

            if (byte_buffer.Size()) {
                out.SetProperty(
                    String("module[") + String::ToString(index) + "]",
                    byte_buffer
                );
            }
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        auto compiled_shader = UniquePtr<CompiledShader>::Construct();

        String name;

        if (auto err = in.GetProperty("name").ReadString(name)) {
            return err;
        }

        UInt num_properties;

        if (auto err = in.GetProperty("properties.size").ReadUnsignedInt(&num_properties)) {
            return err;
        }

        for (UInt i = 0; i < num_properties; i++) {
            const auto param_string = String("properties.") + String::ToString(i);

            String property_name;

            if (auto err = in.GetProperty(param_string + ".name").ReadString(name)) {
                continue;
            }

            compiled_shader->properties.Set(property_name);
        }

        for (SizeType index = 0; index < ShaderModule::Type::MAX; index++) {
            const auto module_property_name = String("module[") + String::ToString(index) + "]";

            if (const auto &property = in.GetProperty(module_property_name)) {
                if (auto err = property.ReadByteBuffer(compiled_shader->modules[static_cast<ShaderModule::Type>(index)])) {
                    return err;
                }
            }
        }
        if (!compiled_shader->IsValid()) {
            return { FBOMResult::FBOM_ERR, "Cannot deserialize invalid compiled shader instance" };
        }

        out_object = std::move(compiled_shader);

        return { FBOMResult::FBOM_OK };
    }
};

template <>
class FBOMMarshaler<CompiledShaderBatch> : public FBOMObjectMarshalerBase<CompiledShaderBatch>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType("CompiledShaderBatch");
    }

    virtual FBOMResult Serialize(const CompiledShaderBatch &in_object, FBOMObject &out) const override
    {
        for (auto &compiled_shader : in_object.compiled_shaders) {
            out.AddChild(compiled_shader);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        auto batch = UniquePtr<CompiledShaderBatch>::Construct();

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("CompiledShader")) {
                auto compiled_shader = node.deserialized.Get<CompiledShader>();
                AssertThrow(compiled_shader != nullptr);

                batch->compiled_shaders.PushBack(*compiled_shader);
            }
        }

        out_object = std::move(batch);

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif