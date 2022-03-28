#ifndef HYPERION_V2_SHADER_H
#define HYPERION_V2_SHADER_H

#include "base.h"

#include <rendering/backend/renderer_shader.h>
#include <rendering/backend/renderer_buffer.h>
#include <rendering/backend/renderer_structs.h>

#include <math/transform.h>
#include <util/heap_array.h>
#include <util/range.h>

#include <memory>

namespace hyperion::v2 {

using renderer::ShaderObject;
using renderer::ShaderModule;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::StorageBuffer;
using renderer::PerFrameData;

struct alignas(256) ObjectShaderData {
    Matrix4 model_matrix;
};

struct alignas(256) MaterialShaderData {
    Vector4 albedo;
    float metalness;
    float roughness;
};

struct alignas(256) SceneShaderData {
    Matrix4 view;
    Matrix4 projection;
    Vector4 camera_position;
    Vector4 light_direction;
};

template <class Buffer, class StructType, size_t Size>
class ShaderData {
public:
    ShaderData(size_t num_buffers)
    {
        m_buffers.resize(num_buffers);
        m_dirty.resize(num_buffers);

        for (size_t i = 0; i < num_buffers; i++) {
            m_buffers[i] = std::make_unique<Buffer>();
            m_dirty[i] = {0, Size};
        }
    }

    ShaderData(const ShaderData &other) = delete;
    ShaderData &operator=(const ShaderData &other) = delete;
    ~ShaderData() = default;

    inline const auto &GetBuffers() const { return m_buffers; }
    
    void Create(Device *device)
    {
        for (size_t i = 0; i < m_buffers.size(); i++) {
            m_buffers[i]->Create(device, m_objects.ByteSize());
        }
    }

    void Destroy(Device *device)
    {
        for (size_t i = 0; i < m_buffers.size(); i++) {
            m_buffers[i]->Destroy(device);
        }
    }

    void UpdateBuffer(Device *device, size_t buffer_index)
    {
        auto &dirty = m_dirty[buffer_index];

        if (!dirty.GetEnd()) {
            return;
        }

        AssertThrow(dirty.GetEnd() > dirty.GetStart());

        m_buffers[buffer_index]->Copy(
            device,
            dirty.GetStart() * sizeof(StructType),
            dirty.GetDistance() * sizeof(StructType),
            m_objects.Data() + dirty.GetStart()
        );

        dirty = {0, 0};
    }

    inline StructType &Get(size_t index) { return m_objects[index]; }
    inline const StructType &Get(size_t index) const { return m_objects[index]; }

    void Set(size_t index, StructType &&value)
    {
        AssertThrow(index < m_objects.Size());

        m_objects[index] = std::move(value);

        for (size_t i = 0; i < m_dirty.size(); i++) {
            m_dirty[i] = {
                MathUtil::Min(m_dirty[i].GetStart(), index),
                MathUtil::Max(m_dirty[i].GetEnd(), index + 1)
            };
        }
    }
    
private:
    std::vector<std::unique_ptr<Buffer>> m_buffers;
    std::vector<Range<size_t>> m_dirty;
    HeapArray<StructType, Size> m_objects;
};

struct ShaderGlobals {
    /* max number of materials, based on size in mb */
    static constexpr size_t max_materials = (1ull * 1024ull * 1024ull) / sizeof(MaterialShaderData);
    static constexpr size_t max_materials_bytes = max_materials * sizeof(MaterialShaderData);
    /* max number of objects, based on size in mb */
    static constexpr size_t max_objects = (1ull * 1024ull * 1024ull) / sizeof(ObjectShaderData);
    static constexpr size_t max_objects_bytes = max_materials * sizeof(ObjectShaderData);

    ShaderGlobals(size_t num_buffers)
        : scene(num_buffers),
          objects(num_buffers),
          materials(num_buffers)
    {
    }

    ShaderData<UniformBuffer, SceneShaderData, 1> scene;
    ShaderData<StorageBuffer, ObjectShaderData, max_objects> objects;
    ShaderData<StorageBuffer, MaterialShaderData, max_materials> materials;
};

struct SubShader {
    ShaderModule::Type type;
    ShaderObject       spirv;
};

class Shader : public EngineComponent<renderer::ShaderProgram> {
public:
    explicit Shader(const std::vector<SubShader> &sub_shaders);
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    ~Shader();

    void Create(Engine *engine);
    void Destroy(Engine *engine);

private:
    std::vector<SubShader> m_sub_shaders;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_SHADER_H

