#ifndef HYPERION_RENDERER_SHADER_H
#define HYPERION_RENDERER_SHADER_H

#include <core/lib/ByteBuffer.hpp>
#include <rendering/backend/RendererDevice.hpp>

#include <asset/ByteReader.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

struct ShaderObject
{
    ByteBuffer bytes;

    struct Metadata
    {
        std::string name;
        std::string path;
    } metadata;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(bytes);
        hc.Add(metadata.name);
        hc.Add(metadata.path);

        return hc;
    }
};

struct ShaderModule
{
    enum Type : UInt
    {
        UNSET = 0,
        VERTEX,
        FRAGMENT,
        GEOMETRY,
        COMPUTE,
        /* Mesh shaders */
        TASK,
        MESH,
        /* Tesselation */
        TESS_CONTROL,
        TESS_EVAL,
        /* Raytracing */
        RAY_GEN,
        RAY_INTERSECT,
        RAY_ANY_HIT,
        RAY_CLOSEST_HIT,
        RAY_MISS,

        MAX
    } type;

    ShaderObject spirv;
    VkShaderModule shader_module;

    ShaderModule(Type type)
        : type(type),
          spirv { },
          shader_module { }
    {
    }

    ShaderModule(Type type, const ShaderObject &spirv, VkShaderModule shader_module = nullptr)
        : type(type),
          spirv(spirv),
          shader_module(shader_module)
    {
    }

    ShaderModule(const ShaderModule &other) = default;
    ~ShaderModule() = default;

    bool operator<(const ShaderModule &other) const
        { return type < other.type; }

    bool IsRaytracing() const
    {
        return type == Type::RAY_GEN
            || type == Type::RAY_INTERSECT
            || type == Type::RAY_ANY_HIT
            || type == Type::RAY_CLOSEST_HIT
            || type == Type::RAY_MISS;
    }
};

struct ShaderGroup
{
    ShaderModule::Type type;
    VkRayTracingShaderGroupCreateInfoKHR raytracing_group_create_info;
};

class ShaderProgram
{
public:
    static constexpr const char *entry_point = "main";

    ShaderProgram();
    ShaderProgram(const ShaderProgram &other) = delete;
    ShaderProgram &operator=(const ShaderProgram &other) = delete;
    ~ShaderProgram();

    const std::vector<ShaderModule> &GetShaderModules() const
        { return m_shader_modules; }

    std::vector<VkPipelineShaderStageCreateInfo> &GetShaderStages()
        { return m_shader_stages; }

    const std::vector<VkPipelineShaderStageCreateInfo> &GetShaderStages() const
        { return m_shader_stages; }

    /* For raytracing only */
    const std::vector<ShaderGroup> &GetShaderGroups() const
        { return m_shader_groups; }

    bool IsRaytracing() const
    {
        return std::any_of(
            m_shader_modules.begin(),
            m_shader_modules.end(),
            [](const ShaderModule &it) {
                return it.IsRaytracing();
            }
        );
    }

    Result AttachShader(Device *device, ShaderModule::Type type, const ShaderObject &spirv);

    Result Create(Device *device);
    Result Destroy(Device *device);

    HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &shader_module : m_shader_modules) {
            hc.Add(static_cast<Int>(shader_module.type));
            hc.Add(shader_module.spirv.GetHashCode());
        }

        return hc;
    }

private:
    VkPipelineShaderStageCreateInfo CreateShaderStage(const ShaderModule &);
    Result CreateShaderGroups();

    std::vector<ShaderModule> m_shader_modules;

    std::vector<VkPipelineShaderStageCreateInfo> m_shader_stages;
    std::vector<ShaderGroup> m_shader_groups;
};

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_SHADER_H
