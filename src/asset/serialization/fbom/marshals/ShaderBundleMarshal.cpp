/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

#include <util/shader_compiler/ShaderCompiler.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypData.hpp>

namespace hyperion::fbom {

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

        // Set global descriptor table version - if this hashcode changes, the shader is invalid and must be recompiled
        out.SetProperty("global_descriptor_table_version", FBOMData::FromUInt64(renderer::g_static_descriptor_table_decl->GetHashCode().Value()));

        out.SetProperty("name", FBOMData::FromName(in_object.definition.name));

        out.SetProperty("entry_point_name", FBOMData::FromString(in_object.entry_point_name));

        const VertexAttributeSet required_vertex_attributes = in_object.definition.properties.GetRequiredVertexAttributes();
        out.SetProperty("required_vertex_attributes", FBOMData::FromUInt64(required_vertex_attributes.flag_mask));

        const VertexAttributeSet optional_vertex_attributes = in_object.definition.properties.GetOptionalVertexAttributes();
        out.SetProperty("optional_vertex_attributes", FBOMData::FromUInt64(optional_vertex_attributes.flag_mask));

        out.SetProperty("num_descriptor_usages", FBOMData::FromUInt32(uint32(in_object.GetDescriptorUsages().Size())));

        for (SizeType index = 0; index < in_object.GetDescriptorUsages().Size(); index++) {
            const DescriptorUsage &item = in_object.GetDescriptorUsages()[index];

            const ANSIString descriptor_name_string(*item.descriptor_name);
            const ANSIString set_name_string(*item.set_name);

            out.SetProperty(
                ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".slot",
                FBOMData::FromUInt32(item.slot)
            );

            out.SetProperty(
                ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".descriptor_name",
                FBOMData::FromString(descriptor_name_string)
            );

            out.SetProperty(
                ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".set_name",
                FBOMData::FromString(set_name_string)
            );

            out.SetProperty(
                ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".flags",
                FBOMData::FromUInt32(item.flags)
            );

            out.SetProperty(
                ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".num_params",
                FBOMData::FromUInt32(uint32(item.params.Size()))
            );

            uint32 param_index = 0;

            for (const auto &it : item.params) {
                out.SetProperty(
                    ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".params[" + ANSIString::ToString(param_index) + "].key",
                    FBOMData::FromString(it.first)
                );

                out.SetProperty(
                    ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".params[" + ANSIString::ToString(param_index) + "].value",
                    FBOMData::FromString(it.second)
                );

                param_index++;
            }
        }

        Array<ShaderProperty> properties_array = in_object.definition.properties.GetPropertySet().ToArray();
        out.SetProperty("properties.size", FBOMData::FromUInt32(uint32(properties_array.Size())));

        for (SizeType index = 0; index < properties_array.Size(); index++) {
            const ShaderProperty &item = properties_array[index];

            // @TODO: Serialize `value`

            out.SetProperty(
                ANSIString("properties.") + ANSIString::ToString(index) + ".name",
                FBOMData::FromString(item.name)
            );

            out.SetProperty(
                ANSIString("properties.") + ANSIString::ToString(index) + ".is_permutation",
                FBOMData::FromBool(item.is_permutation)
            );

            out.SetProperty(
                ANSIString("properties.") + ANSIString::ToString(index) + ".flags",
                FBOMData::FromUInt32(item.flags)
            );

            out.SetProperty(
                ANSIString("properties.") + ANSIString::ToString(index) + ".is_value_group",
                FBOMData::FromBool(item.IsValueGroup())
            );

            if (item.IsValueGroup()) {
                out.SetProperty(
                    ANSIString("properties.") + ANSIString::ToString(index) + ".num_possible_values",
                    FBOMData::FromUInt32(uint32(item.possible_values.Size()))
                );

                for (SizeType i = 0; i < item.possible_values.Size(); i++) {
                    out.SetProperty(
                        ANSIString("properties.") + ANSIString::ToString(index) + ".possible_values[" + ANSIString::ToString(i) + "]",
                        FBOMData::FromString(item.possible_values[i])
                    );
                }
            }
        }

        for (SizeType index = 0; index < in_object.modules.Size(); index++) {
            const ByteBuffer &byte_buffer = in_object.modules[index];

            if (byte_buffer.Size()) {
                out.SetProperty(ANSIString("module[") + ANSIString::ToString(index) + "]", FBOMData::FromByteBuffer(byte_buffer));
            }
        }

        // HYP_LOG(Serialization, LogLevel::INFO, "Serialized shader '{}' with properties:", in_object.definition.name);

        // String properties_string;

        // for (const ShaderProperty &property : properties_array) {
        //     properties_string += "\t" + property.name + "\n";
        // }

        // HYP_LOG(Serialization, LogLevel::INFO, "\t{}", properties_string);

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        uint64 global_descriptor_table_version = -1;

        if (FBOMResult err = in.GetProperty("global_descriptor_table_version").ReadUInt64(&global_descriptor_table_version)) {
            return err;
        }

        if (global_descriptor_table_version != renderer::g_static_descriptor_table_decl->GetHashCode().Value()) {
            DebugLog(
                LogType::Error,
                "Failed to deserialize Shader instance: The global descriptor table version does not match.\n"
                "\tExpected: %llu\n"
                "\tActual: %llu\n",
                renderer::g_static_descriptor_table_decl->GetHashCode().Value(),
                global_descriptor_table_version
            );

            return { FBOMResult::FBOM_ERR, "Global descriptor table version mismatch" };
        }

        CompiledShader compiled_shader;

        if (FBOMResult err = in.GetProperty("name").ReadName(&compiled_shader.definition.name)) {
            return err;
        }

        if (in.HasProperty("entry_point_name")) {
            if (FBOMResult err = in.GetProperty("entry_point_name").ReadString(compiled_shader.entry_point_name)) {
                return err;
            }
        } else {
            compiled_shader.entry_point_name = "main";
        }

        VertexAttributeSet required_vertex_attributes { };

        if (FBOMResult err = in.GetProperty("required_vertex_attributes").ReadUInt64(&required_vertex_attributes.flag_mask)) {
            return err;
        }

        compiled_shader.definition.properties.SetRequiredVertexAttributes(required_vertex_attributes);

        VertexAttributeSet optional_vertex_attributes { };
        
        if (FBOMResult err = in.GetProperty("optional_vertex_attributes").ReadUInt64(&optional_vertex_attributes.flag_mask)) {
            return err;
        }

        compiled_shader.definition.properties.SetOptionalVertexAttributes(optional_vertex_attributes);

        uint32 num_descriptor_usages = 0;

        if (in.HasProperty("num_descriptor_usages")) {
           in.GetProperty("num_descriptor_usages").ReadUInt32(&num_descriptor_usages);

            if (num_descriptor_usages != 0) {
                for (uint32 i = 0; i < num_descriptor_usages; i++) {
                    const ANSIString descriptor_usage_index_string = ANSIString("descriptor_usages.") + ANSIString::ToString(i);

                    DescriptorUsage usage;

                    ANSIString descriptor_name_string;
                    ANSIString set_name_string;

                    if (FBOMResult err = in.GetProperty(descriptor_usage_index_string + ".slot").ReadUInt32(&usage.slot)) {
                        return err;
                    }

                    if (FBOMResult err = in.GetProperty(descriptor_usage_index_string + ".descriptor_name").ReadString(descriptor_name_string)) {
                        return err;
                    }

                    if (FBOMResult err = in.GetProperty(descriptor_usage_index_string + ".set_name").ReadString(set_name_string)) {
                        return err;
                    }

                    usage.descriptor_name = CreateNameFromDynamicString(descriptor_name_string);
                    usage.set_name = CreateNameFromDynamicString(set_name_string);

                    in.GetProperty(descriptor_usage_index_string + ".flags").ReadUInt32(&usage.flags);

                    uint32 num_params = 0;
                    in.GetProperty(descriptor_usage_index_string + ".num_params").ReadUInt32(&num_params);

                    for (uint32 j = 0; j < num_params; j++) {
                        const ANSIString param_string = descriptor_usage_index_string + ".params[" + ANSIString::ToString(j) + "]";

                        String key;
                        String value;

                        if (FBOMResult err = in.GetProperty(param_string + ".key").ReadString(key)) {
                            HYP_LOG(Serialization, LogLevel::ERR, "Failed to read key for descriptor usage 'key' parameter {}", param_string);
                            
                            return err;
                        }

                        if (FBOMResult err = in.GetProperty(param_string + ".value").ReadString(value)) {
                            HYP_LOG(Serialization, LogLevel::ERR, "Failed to read value for descriptor usage 'value' parameter {}", param_string);

                            return err;
                        }

                        usage.params[key] = value;
                    }

                    compiled_shader.GetDescriptorUsages().Add(std::move(usage));
                }
            }
        }

        uint32 num_properties;

        if (FBOMResult err = in.GetProperty("properties.size").ReadUInt32(&num_properties)) {
            return err;
        }

        for (uint32 i = 0; i < num_properties; i++) {
            const ANSIString param_string = ANSIString("properties.") + ANSIString::ToString(i);

            ShaderProperty property;

            if (FBOMResult err = in.GetProperty(param_string + ".name").ReadString(property.name)) {
                continue;
            }

            bool is_value_group = false;

            in.GetProperty(param_string + ".is_permutation").ReadBool(&property.is_permutation);
            in.GetProperty(param_string + ".flags").ReadUInt32(&property.flags);
            in.GetProperty(param_string + ".is_value_group").ReadBool(&is_value_group);

            if (is_value_group) {
                uint32 num_possible_values = 0;

                if (FBOMResult err = in.GetProperty(param_string + ".num_possible_values").ReadUInt32(&num_possible_values)) {
                    continue;
                }

                for (uint32 i = 0; i < num_possible_values; i++) {
                    String possible_value;

                    if (FBOMResult err = in.GetProperty(param_string + ".possible_values[" + ANSIString::ToString(i) + "]").ReadString(possible_value)) {
                        continue;
                    }

                    property.possible_values.PushBack(std::move(possible_value));
                }
            }

            compiled_shader.definition.properties.Set(property);
        }

        for (SizeType index = 0; index < ShaderModuleType::MAX; index++) {
            const ANSIString module_property_name = ANSIString("module[") + ANSIString::ToString(index) + "]";

            if (const FBOMData &property = in.GetProperty(module_property_name)) {
                if (FBOMResult err = property.ReadByteBuffer(compiled_shader.modules[index])) {
                    return err;
                }
            }
        }

        // String properties_string;

        // for (const ShaderProperty &property : compiled_shader.definition.properties.ToArray()) {
        //     properties_string += " " + property.name + "\n";
        // }
        
        // DebugLog(LogType::Info, "Deserialized shader '%s', %u descriptor sets; Properties = %s\n", compiled_shader.definition.name.LookupString(),
        //     compiled_shader.descriptor_usages.Size(),
        //     properties_string.Data());

        out = HypData(std::move(compiled_shader));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(CompiledShader, FBOMMarshaler<CompiledShader>);

template <>
class FBOMMarshaler<CompiledShaderBatch> : public FBOMObjectMarshalerBase<CompiledShaderBatch>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const CompiledShaderBatch &in_object, FBOMObject &out) const override
    {
        for (const CompiledShader &compiled_shader : in_object.compiled_shaders) {
            out.AddChild(compiled_shader);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        CompiledShaderBatch batch;

        for (FBOMObject &subobject : *in.nodes) {
            if (subobject.GetType().IsOrExtends("CompiledShader")) {
                Optional<CompiledShader &> compiled_shader_opt = subobject.m_deserialized_object->TryGet<CompiledShader>();

                if (compiled_shader_opt.HasValue()) {
                    batch.compiled_shaders.PushBack(*compiled_shader_opt);
                } else {
                    HYP_LOG(Serialization, LogLevel::ERR, "Failed to deserialize CompiledShader instance");
                }
            }
        }

        out = HypData(std::move(batch));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(CompiledShaderBatch, FBOMMarshaler<CompiledShaderBatch>);

} // namespace hyperion::fbom