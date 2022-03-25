#ifndef HYPERION_V2_SHADER_H
#define HYPERION_V2_SHADER_H

#include "base.h"

#include <rendering/backend/renderer_shader.h>

#include <math/transform.h>
#include <util/heap_array.h>

#include <memory>

namespace hyperion::v2 {

using renderer::ShaderObject;
using renderer::ShaderModule;

struct alignas(256) ObjectShaderData {
    Matrix4 model_matrix;
};

struct alignas(256) MaterialShaderData {
    Vector4 albedo;
    float metalness;
    float roughness;
};

struct ShaderStorageData {
    /* max number of materials, based on size in mb */
    static constexpr size_t max_materials = (1ull * 1024ull * 1024ull) / sizeof(MaterialShaderData);
    static constexpr size_t max_materials_bytes = max_materials * sizeof(MaterialShaderData);
    /* max number of objects, based on size in mb */
    static constexpr size_t max_objects = (1ull * 1024ull * 1024ull) / sizeof(ObjectShaderData);
    static constexpr size_t max_objects_bytes = max_materials * sizeof(ObjectShaderData);

    HeapArray<ObjectShaderData, max_objects> objects;
    HeapArray<MaterialShaderData, max_materials> materials;

    size_t material_index = 0;

    size_t dirty_object_range_start = 0,
           dirty_object_range_end = 0;
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

