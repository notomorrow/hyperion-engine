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

        out.SetProperty("name", FBOMName(), in_object.GetDefinition().name);

        out.SetProperty("entry_point_name", FBOMString(in_object.entry_point_name.Size()), in_object.entry_point_name.Data());

        const VertexAttributeSet required_vertex_attributes = in_object.GetDefinition().GetProperties().GetRequiredVertexAttributes();
        out.SetProperty("required_vertex_attributes", FBOMData::FromUInt64(required_vertex_attributes.flag_mask));

        const VertexAttributeSet optional_vertex_attributes = in_object.GetDefinition().GetProperties().GetOptionalVertexAttributes();
        out.SetProperty("optional_vertex_attributes", FBOMData::FromUInt64(optional_vertex_attributes.flag_mask));

        auto properties_array = in_object.GetDefinition().properties.GetPropertySet().ToArray();

        out.SetProperty("properties.size", FBOMData::FromUInt32(UInt32(properties_array.Size())));

        for (SizeType index = 0; index < properties_array.Size(); index++) {
            const ShaderProperty &item = properties_array[index];

            out.SetProperty(
                String("properties.") + String::ToString(index) + ".name",
                FBOMData::FromString(item.name)
            );

            out.SetProperty(
                String("properties.") + String::ToString(index) + ".is_permutation",
                FBOMData::FromBool(item.is_permutation)
            );

            out.SetProperty(
                String("properties.") + String::ToString(index) + ".flags",
                FBOMData::FromUInt32(item.flags)
            );

            out.SetProperty(
                String("properties.") + String::ToString(index) + ".is_value_group",
                FBOMData::FromBool(item.IsValueGroup())
            );

            if (item.IsValueGroup()) {
                out.SetProperty(
                    String("properties.") + String::ToString(index) + ".num_possible_values",
                    FBOMData::FromUInt32(UInt32(item.possible_values.Size()))
                );

                for (SizeType i = 0; i < item.possible_values.Size(); i++) {
                    out.SetProperty(
                        String("properties.") + String::ToString(index) + ".possible_values[" + String::ToString(i) + "]",
                        FBOMData::FromString(item.possible_values[i])
                    );
                }
            }
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

        Name name;

        if (auto err = in.GetProperty("name").ReadName(&name)) {
            return err;
        }

        if (in.HasProperty("entry_point_name")) {
            if (auto err = in.GetProperty("entry_point_name").ReadString(compiled_shader->entry_point_name)) {
                return err;
            }
        } else {
            compiled_shader->entry_point_name = "main";
        }

        VertexAttributeSet required_vertex_attributes;
        in.GetProperty("required_vertex_attributes").ReadUInt64(&required_vertex_attributes.flag_mask);

        VertexAttributeSet optional_vertex_attributes;
        in.GetProperty("optional_vertex_attributes").ReadUInt64(&optional_vertex_attributes.flag_mask);

        compiled_shader->GetDefinition().GetProperties().SetRequiredVertexAttributes(required_vertex_attributes);
        compiled_shader->GetDefinition().GetProperties().SetOptionalVertexAttributes(optional_vertex_attributes);

        UInt num_properties;

        if (auto err = in.GetProperty("properties.size").ReadUInt32(&num_properties)) {
            return err;
        }

        for (UInt i = 0; i < num_properties; i++) {
            const auto param_string = String("properties.") + String::ToString(i);

            ShaderProperty property;

            if (auto err = in.GetProperty(param_string + ".name").ReadString(property.name)) {
                continue;
            }

            bool is_value_group = false;

            in.GetProperty(param_string + ".is_permutation").ReadBool(&property.is_permutation);
            in.GetProperty(param_string + ".flags").ReadUInt32(&property.flags);
            in.GetProperty(param_string + ".is_value_group").ReadBool(&is_value_group);

            if (is_value_group) {
                UInt32 num_possible_values = 0;

                if (auto err = in.GetProperty(param_string + ".num_possible_values").ReadUInt32(&num_possible_values)) {
                    continue;
                }

                for (UInt32 i = 0; i < num_possible_values; i++) {
                    String possible_value;

                    if (auto err = in.GetProperty(param_string + ".possible_values[" + String::ToString(i) + "]").ReadString(possible_value)) {
                        continue;
                    }

                    property.possible_values.PushBack(std::move(possible_value));
                }
            }

            compiled_shader->GetDefinition().properties.Set(property);
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