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
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const CompiledShader &in_object, FBOMObject &out) const override
    {
        if (!in_object.IsValid()) {
            return { FBOMResult::FBOM_ERR, "Cannot serialize invalid compiled shader instance" };
        }

        out.SetProperty("name", FBOMName(), in_object.GetDefinition().name);

        out.SetProperty("entry_point_name", FBOMString(in_object.entry_point_name.Size()), in_object.entry_point_name.Data());

        const VertexAttributeSet required_vertex_attributes = in_object.GetDefinition().GetProperties().GetRequiredVertexAttributes();
        out.SetProperty("required_vertex_attributes", FBOMData::FromUnsignedLong(required_vertex_attributes.flag_mask));

        const VertexAttributeSet optional_vertex_attributes = in_object.GetDefinition().GetProperties().GetOptionalVertexAttributes();
        out.SetProperty("optional_vertex_attributes", FBOMData::FromUnsignedLong(optional_vertex_attributes.flag_mask));

        out.SetProperty("num_descriptor_usages", FBOMData::FromUnsignedInt(uint32(in_object.GetDefinition().GetDescriptorUsages().Size())));

        for (SizeType index = 0; index < in_object.GetDefinition().GetDescriptorUsages().Size(); index++) {
            const DescriptorUsage &item = in_object.GetDefinition().GetDescriptorUsages()[index];

            out.SetProperty(
                String("descriptor_usages.") + String::ToString(index) + ".slot",
                FBOMData::FromUnsignedInt(item.slot)
            );

            out.SetProperty(
                String("descriptor_usages.") + String::ToString(index) + ".descriptor_name",
                FBOMString(item.descriptor_name.Size()),
                item.descriptor_name.Data()
            );

            out.SetProperty(
                String("descriptor_usages.") + String::ToString(index) + ".set_name",
                FBOMName(),
                item.set_name
            );

            out.SetProperty(
                String("descriptor_usages.") + String::ToString(index) + ".flags",
                FBOMData::FromUnsignedInt(item.flags)
            );

            out.SetProperty(
                String("descriptor_usages.") + String::ToString(index) + ".num_params",
                FBOMData::FromUnsignedInt(uint32(item.params.Size()))
            );

            uint param_index = 0;

            for (const auto &it : item.params) {
                out.SetProperty(
                    String("descriptor_usages.") + String::ToString(index) + ".params[" + String::ToString(param_index) + "].key",
                    FBOMData::FromString(it.first)
                );

                out.SetProperty(
                    String("descriptor_usages.") + String::ToString(index) + ".params[" + String::ToString(param_index) + "].value",
                    FBOMData::FromString(it.second)
                );

                param_index++;
            }
        }

        auto properties_array = in_object.GetDefinition().properties.GetPropertySet().ToArray();
        out.SetProperty("properties.size", FBOMData::FromUnsignedInt(uint32(properties_array.Size())));

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
                FBOMData::FromUnsignedInt(item.flags)
            );

            out.SetProperty(
                String("properties.") + String::ToString(index) + ".is_value_group",
                FBOMData::FromBool(item.IsValueGroup())
            );

            if (item.IsValueGroup()) {
                out.SetProperty(
                    String("properties.") + String::ToString(index) + ".num_possible_values",
                    FBOMData::FromUnsignedInt(uint32(item.possible_values.Size()))
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
        in.GetProperty("required_vertex_attributes").ReadUnsignedLong(&required_vertex_attributes.flag_mask);

        VertexAttributeSet optional_vertex_attributes;
        in.GetProperty("optional_vertex_attributes").ReadUnsignedLong(&optional_vertex_attributes.flag_mask);

        compiled_shader->GetDefinition().GetProperties().SetRequiredVertexAttributes(required_vertex_attributes);
        compiled_shader->GetDefinition().GetProperties().SetOptionalVertexAttributes(optional_vertex_attributes);

        uint num_descriptor_usages = 0;

        if (in.HasProperty("num_descriptor_usages")) {
            in.GetProperty("num_descriptor_usages").ReadUnsignedInt(&num_descriptor_usages);

            if (num_descriptor_usages != 0) {
                for (uint i = 0; i < num_descriptor_usages; i++) {
                    const auto descriptor_usage_index_string = String("descriptor_usages.") + String::ToString(i);

                    DescriptorUsage usage;

                    if (auto err = in.GetProperty(descriptor_usage_index_string + ".slot").ReadUnsignedInt(&usage.slot)) {
                        return err;
                    }

                    if (auto err = in.GetProperty(descriptor_usage_index_string + ".descriptor_name").ReadString(usage.descriptor_name)) {
                        return err;
                    }

                    if (auto err = in.GetProperty(descriptor_usage_index_string + ".set_name").ReadName(&usage.set_name)) {
                        return err;
                    }

                    in.GetProperty(descriptor_usage_index_string + ".flags").ReadUnsignedInt(&usage.flags);

                    uint num_params = 0;
                    in.GetProperty(descriptor_usage_index_string + ".num_params").ReadUnsignedInt(&num_params);

                    for (uint j = 0; j < num_params; j++) {
                        const String param_string = descriptor_usage_index_string + ".params[" + String::ToString(j) + "]";

                        String key;
                        String value;

                        if (auto err = in.GetProperty(param_string + ".key").ReadString(key)) {
                            return err;
                        }

                        if (auto err = in.GetProperty(param_string + ".value").ReadString(value)) {
                            return err;
                        }

                        usage.params[key] = value;
                    }

                    compiled_shader->GetDefinition().GetDescriptorUsages().Add(std::move(usage));
                }
            }
        }

        uint num_properties;

        if (auto err = in.GetProperty("properties.size").ReadUnsignedInt(&num_properties)) {
            return err;
        }

        for (uint i = 0; i < num_properties; i++) {
            const auto param_string = String("properties.") + String::ToString(i);

            ShaderProperty property;

            if (auto err = in.GetProperty(param_string + ".name").ReadString(property.name)) {
                continue;
            }

            bool is_value_group = false;

            in.GetProperty(param_string + ".is_permutation").ReadBool(&property.is_permutation);
            in.GetProperty(param_string + ".flags").ReadUnsignedInt(&property.flags);
            in.GetProperty(param_string + ".is_value_group").ReadBool(&is_value_group);

            if (is_value_group) {
                uint32 num_possible_values = 0;

                if (auto err = in.GetProperty(param_string + ".num_possible_values").ReadUnsignedInt(&num_possible_values)) {
                    continue;
                }

                for (uint32 i = 0; i < num_possible_values; i++) {
                    String possible_value;

                    if (auto err = in.GetProperty(param_string + ".possible_values[" + String::ToString(i) + "]").ReadString(possible_value)) {
                        continue;
                    }

                    property.possible_values.PushBack(std::move(possible_value));
                }
            }

            compiled_shader->GetDefinition().properties.Set(property);
        }

        for (SizeType index = 0; index < ShaderModuleType::MAX; index++) {
            const auto module_property_name = String("module[") + String::ToString(index) + "]";

            if (const auto &property = in.GetProperty(module_property_name)) {
                if (auto err = property.ReadByteBuffer(compiled_shader->modules[index])) {
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
    virtual ~FBOMMarshaler() override = default;

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