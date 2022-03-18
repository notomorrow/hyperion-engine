//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_SHADER_H
#define HYPERION_RENDERER_SHADER_H

#include "./spirv.h"
#include "../../hash_code.h"
#include "renderer_device.h"

namespace hyperion {
namespace renderer {
struct ShaderModule {
    SpirvObject spirv;
    VkShaderModule shader_module;

    ShaderModule()
        : spirv{}, shader_module{} {}
    ShaderModule(const SpirvObject &spirv, VkShaderModule shader_module = nullptr)
        : spirv(spirv), shader_module(shader_module) {}
    ShaderModule(const ShaderModule &other)
        : spirv(other.spirv), shader_module(other.shader_module) {}
    ~ShaderModule() = default;

    inline bool operator<(const ShaderModule &other) const
    {
        return spirv.type < other.spirv.type;
    }
};

class ShaderProgram {
public:
    ShaderProgram();
    ShaderProgram(const ShaderProgram &other) = delete;
    ShaderProgram &operator=(const ShaderProgram &other) = delete;
    ~ShaderProgram();

    void AttachShader(Device *device, const SpirvObject &spirv);
    //void AttachShader(Device *device, ShaderType type, const uint32_t *code, const size_t code_size);
    VkPipelineShaderStageCreateInfo CreateShaderStage(const ShaderModule &module, const char *entry_point);
    void CreateProgram(const char *entry_point);
    Result Destroy(Device *device);

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &shader_module : shader_modules) {
            hc.Add(shader_module.spirv.GetHashCode());
        }

        return hc;
    }

private:
    std::vector<ShaderModule> shader_modules;
};

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_SHADER_H
