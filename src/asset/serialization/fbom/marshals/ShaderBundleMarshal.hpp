#ifndef HYPERION_V2_FBOM_MARSHALS_SHADER_BUNDLE_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_SHADER_BUNDLE_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <builders/shader_compiler/ShaderCompiler.hpp>
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
        out.SetProperty("version", FBOMUnsignedLong(), in_object.version_hash);

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

    virtual FBOMResult Deserialize(Engine *, const FBOMObject &in, UniquePtr<CompiledShader> &out_object) const override
    {
        out_object.Reset(new CompiledShader);

        if (auto err = in.GetProperty("version").ReadUnsignedLong(&out_object->version_hash)) {
            return err;
        }

        for (SizeType index = 0; index < ShaderModule::Type::MAX; index++) {
            const auto module_property_name = String("module[") + String::ToString(index) + "]";

            if (const auto &property = in.GetProperty(module_property_name)) {
                if (auto err = property.ReadByteBuffer(out_object->modules[static_cast<ShaderModule::Type>(index)])) {
                    return err;
                }
            }
        }

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

        for (auto &compiled_shader : in_object.compiled_shaders) {
            std::cout << "compiled_shader : " << compiled_shader.version_hash << "\n";
            std::cout << "compiled_shader : " << compiled_shader.modules.Size() << "\n";
            for (auto &mod : compiled_shader.modules) {
                std::cout << "size : " << mod.Size() << "\n";
            }
            std::cout << "\n";
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(Engine *, const FBOMObject &in, UniquePtr<CompiledShaderBatch> &out_object) const override
    {
        out_object.Reset(new CompiledShaderBatch);

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("CompiledShader")) {
                auto compiled_shader = node.deserialized.Get<CompiledShader>();
                AssertThrow(compiled_shader != nullptr);

                out_object->compiled_shaders.PushBack(*compiled_shader);
            }
        }

        for (auto &compiled_shader : out_object->compiled_shaders) {
            std::cout << "compiled_shader : " << compiled_shader.version_hash << "\n";
            std::cout << "compiled_shader : " << compiled_shader.modules.Size() << "\n";
            for (auto &mod : compiled_shader.modules) {
                std::cout << "size : " << mod.Size() << "\n";
            }
            std::cout << "\n";
        }

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif