//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_SHADER_H
#define HYPERION_RENDERER_SHADER_H

#include "../backend/spirv.h"
#include "../../hash_code.h"
#include "renderer_device.h"

namespace hyperion {

struct RendererShaderModule {
    SpirvObject spirv;
    VkShaderModule shader_module;

    RendererShaderModule()
        : spirv{}, shader_module{} {}
    RendererShaderModule(const SpirvObject &spirv, VkShaderModule shader_module = nullptr)
        : spirv(spirv), shader_module(shader_module) {}
    RendererShaderModule(const RendererShaderModule &other)
        : spirv(other.spirv), shader_module(other.shader_module) {}
    ~RendererShaderModule() = default;

    inline bool operator<(const RendererShaderModule &other) const
        { return spirv.type < other.spirv.type; }
};

class RendererShader {
public:
    void AttachShader(RendererDevice *device, const SpirvObject &spirv);
    //void AttachShader(RendererDevice *device, ShaderType type, const uint32_t *code, const size_t code_size);
    VkPipelineShaderStageCreateInfo CreateShaderStage(const RendererShaderModule &module, const char *entry_point);
    void CreateProgram(const char *entry_point);
    void Destroy();

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
    std::vector<RendererShaderModule> shader_modules;
    RendererDevice *device = nullptr;
};

}; // namespace hyperion

#endif //HYPERION_RENDERER_SHADER_H
