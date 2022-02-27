#ifndef SHADER_H
#define SHADER_H

#include "../math/vector2.h"
#include "../math/vector3.h"
#include "../math/vector4.h"
#include "../math/matrix4.h"
#include "../math/transform.h"
#include "../asset/loadable.h"
#include "../hash_code.h"
#include "../util.h"
#include "../util/non_owning_ptr.h"
#include "material.h"
#include "uniform.h"
#include "camera/camera.h"

#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <string>
#include <cstring>

namespace hyperion {
class Texture;
class Environment;

class ShaderProperties {
public:
    struct ShaderProperty {
        std::string raw_value;

        union Value {
            float float_value;
            int int_value;
            bool bool_value;
        } value;

        enum ShaderPropertyType {
            SHADER_PROPERTY_UNSET = 0,
            SHADER_PROPERTY_STRING,
            SHADER_PROPERTY_FLOAT,
            SHADER_PROPERTY_INT,
            SHADER_PROPERTY_BOOL
        } type;

        ShaderProperty()
            : type(SHADER_PROPERTY_UNSET),
              raw_value("")
        {
            value.int_value = 0;
        }

        ShaderProperty(const ShaderProperty &other)
            : type(other.type),
              raw_value(other.raw_value),
              value(other.value)
        {
        }

        inline ShaderProperty &operator=(const ShaderProperty &other)
        {
            type = other.type;
            raw_value = other.raw_value;
            value = other.value;

            return *this;
        }

        inline bool operator==(const ShaderProperty &other) const
        {
            return type == other.type &&
                raw_value == other.raw_value;
        }

        inline bool IsTruthy() const
        {
            switch (type) {
            case SHADER_PROPERTY_UNSET:
                return false;
            case SHADER_PROPERTY_STRING:
                return true;
            case SHADER_PROPERTY_INT:
                return value.int_value != 0;
            case SHADER_PROPERTY_FLOAT:
                return !MathUtil::Approximately(value.float_value, 0.0f);
            case SHADER_PROPERTY_BOOL:
                return !!value.bool_value;
            default:
                return false;
            }
        }

        inline HashCode GetHashCode() const
        {
            HashCode hc;

            hc.Add(int(type));
            hc.Add(raw_value);

            return hc;
        }
    };

    ShaderProperties() {}
    ShaderProperties(const ShaderProperties &other)
        : m_properties(other.m_properties)
    {
    }

    std::unordered_map<std::string, ShaderProperty> m_properties;

    inline bool operator==(const ShaderProperties &other) const
    {
        return m_properties == other.m_properties;
    }

    inline bool HasValue(const std::string &key) const
    {
        auto it = m_properties.find(key);

        return it != m_properties.end();
    }

    inline ShaderProperty GetValue(const std::string &key) const
    {
        ShaderProperty result;
        result.value.int_value = 0;
        result.type = ShaderProperty::SHADER_PROPERTY_UNSET;

        auto it = m_properties.find(key);

        if (it != m_properties.end()) {
            result = it->second;
        }

        return result;
    }

    inline ShaderProperties &Define(const std::string &key, const std::string &value)
    {
        ShaderProperty shader_property;
        shader_property.raw_value = value;
        shader_property.type = ShaderProperty::ShaderPropertyType::SHADER_PROPERTY_STRING;

        m_properties[key] = shader_property;

        return *this;
    }

    inline ShaderProperties &Define(const std::string &key, int value)
    {
        ShaderProperty shader_property;
        shader_property.raw_value = std::to_string(value);
        shader_property.value.int_value = value;
        shader_property.type = ShaderProperty::ShaderPropertyType::SHADER_PROPERTY_INT;

        m_properties[key] = shader_property;

        return *this;
    }

    inline ShaderProperties &Define(const std::string &key, float value)
    {
        ShaderProperty shader_property;
        shader_property.raw_value = std::to_string(value);
        shader_property.value.float_value = value;
        shader_property.type = ShaderProperty::ShaderPropertyType::SHADER_PROPERTY_FLOAT;

        m_properties[key] = shader_property;

        return *this;
    }

    inline ShaderProperties &Define(const std::string &key, bool value)
    {
        ShaderProperty shader_property;
        shader_property.raw_value = std::to_string(value);
        shader_property.value.bool_value = value;
        shader_property.type = ShaderProperty::ShaderPropertyType::SHADER_PROPERTY_BOOL;

        m_properties[key] = shader_property;

        return *this;
    }

    inline ShaderProperties &Merge(const ShaderProperties &other)
    {
        for (auto &it : other.m_properties) {
            m_properties[it.first] = it.second;
        }

        return *this;
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &it : m_properties) {
            hc.Add(it.first);
            hc.Add(it.second.GetHashCode());
        }

        return hc;
    }
};

class Shader {
    friend class Renderer;
public:

    struct UniformResult;
    struct UniformBufferResult;

    struct DeclaredUniform {
        using Id_t = int;

        Id_t id;
        std::string name;
        Uniform value;

        DeclaredUniform(Id_t id, const std::string &name, Uniform value = Uniform())
            : id(id), name(name), value(value)
        {
        }

        DeclaredUniform(const DeclaredUniform &other)
            : id(other.id), name(other.name), value(other.value)
        {
        }

        inline DeclaredUniform &operator=(const DeclaredUniform &other)
        {
            id = other.id;
            name = other.name;
            value = other.value;

            return *this;
        }
    };

    struct UniformBuffer {
        using Id_t = int;

        Id_t id;
        std::string name;
        std::vector<DeclaredUniform> data;

        struct Internal {
            using Handle_t = unsigned int;

            Handle_t handle;
            size_t size;
            size_t index;
            bool generated;
        };

        non_owning_ptr<Internal> _internal;

        UniformBuffer(Id_t id, const std::string &name)
            : id(id),
              name(name)
        {
        }

        UniformBuffer(const UniformBuffer &other)
            : id(other.id),
              name(other.name),
              data(other.data),
              _internal(other._internal)
        {
        }

        inline UniformBuffer &operator=(const UniformBuffer &other)
        {
            id = other.id;
            name = other.name;
            data = other.data;
            _internal = other._internal;

            return *this;
        }

        UniformResult Acquire(const std::string &name)
        {
            DeclaredUniform::Id_t id = data.size();
            data.push_back(DeclaredUniform(id, name));

            return UniformResult(UniformResult::DECLARED_UNIFORM_OK, id);
        }

        inline void Set(DeclaredUniform::Id_t id, const Uniform &uniform)
        {
            ex_assert(id >= 0);
            ex_assert(id < data.size());

            data[id].value = uniform;
        }
    };

    struct DirectionalLightUniform {
        DeclaredUniform::Id_t uniform_color,
            uniform_direction,
            uniform_intensity;
    };

    struct PointLightUniform {
        DeclaredUniform::Id_t uniform_color,
            uniform_position,
            uniform_radius,
            uniform_intensity;
    };

    class DeclaredUniforms {
    public:

        DeclaredUniforms() { m_uniforms.reserve(32); }
        DeclaredUniforms(const DeclaredUniforms &other) = delete;
        inline DeclaredUniforms &operator=(const DeclaredUniforms &other) = delete;

        UniformBufferResult AcquireBuffer(const std::string &name)
        {
            UniformBuffer::Id_t id = m_uniform_buffers.size();
            m_uniform_buffers.push_back(std::make_pair(UniformBuffer(id, name), true));

            return UniformBufferResult(UniformBufferResult::DECLARED_UNIFORM_BUFFER_OK, id);
        }

        UniformResult Acquire(const std::string &name)
        {
            DeclaredUniform::Id_t id = m_uniforms.size();
            m_uniforms.push_back(std::make_pair(DeclaredUniform(id, name), true));
 
            return UniformResult(UniformResult::DECLARED_UNIFORM_OK, id);
        }

        UniformResult Acquire(UniformBuffer::Id_t buffer_id, const std::string &name)
        {
            ex_assert(buffer_id >= 0);
            ex_assert(buffer_id < m_uniform_buffers.size());

            auto &buffer = m_uniform_buffers[buffer_id].first;

            return buffer.Acquire(name);
        }

        inline void Set(DeclaredUniform::Id_t id, const Uniform &uniform)
        {
            ex_assert(id >= 0);
            ex_assert(id < m_uniforms.size());

            m_uniforms[id].first.value = uniform;
            m_uniforms[id].second = true;
        }

        inline void Set(UniformBuffer::Id_t buffer_id, DeclaredUniform::Id_t uniform_id, const Uniform &uniform)
        {
            ex_assert(buffer_id >= 0);
            ex_assert(buffer_id < m_uniform_buffers.size());

            auto &buffer = m_uniform_buffers[buffer_id].first;

            ex_assert(uniform_id >= 0);
            ex_assert(uniform_id < buffer.data.size());

            buffer.data[uniform_id].value = uniform;

            m_uniform_buffers[buffer_id].second = true; // set changed to true
        }

        // bool - has changed?
        std::vector<std::pair<DeclaredUniform, bool>> m_uniforms;
        std::vector<std::pair<UniformBuffer, bool>> m_uniform_buffers;
    };
    struct UniformResult {
        enum {
            DECLARED_UNIFORM_OK,
            DECLARED_UNIFORM_ERR
        } result;

        DeclaredUniform::Id_t id;

        std::string message;

        UniformResult(decltype(result) result, DeclaredUniform::Id_t id = -1, const std::string &message = "")
            : result(result), message(message), id(id) {}
        UniformResult(const UniformResult &other)
            : result(other.result), message(other.message), id(other.id) {}
        inline UniformResult &operator=(const UniformResult &other)
        {
            result = other.result;
            id = other.id;
            message = other.message;

            return *this;
        }

        inline explicit operator bool() const { return result == DECLARED_UNIFORM_OK; }
    };

    struct UniformBufferResult {
        enum {
            DECLARED_UNIFORM_BUFFER_OK,
            DECLARED_UNIFORM_BUFFER_ERR
        } result;

        UniformBuffer::Id_t id;

        std::string message;

        UniformBufferResult(decltype(result) result, UniformBuffer::Id_t id = -1, const std::string &message = "")
            : result(result), message(message), id(id) {}
        UniformBufferResult(const UniformBufferResult &other)
            : result(other.result), message(other.message), id(other.id) {}
        inline UniformBufferResult &operator=(const UniformBufferResult &other)
        {
            result = other.result;
            id = other.id;
            message = other.message;

            return *this;
        }

        inline explicit operator bool() const { return result == DECLARED_UNIFORM_BUFFER_OK; }
    };


    Shader(const ShaderProperties &properties);
    Shader(const ShaderProperties &properties,
        const std::string &vscode,
        const std::string &fscode);
    Shader(const Shader &other) = delete;
    Shader &operator=(const Shader &other) = delete;
    virtual ~Shader();

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);

    inline int GetId() const { return progid; }

    inline ShaderProperties &GetProperties() { return m_properties; }
    inline const ShaderProperties &GetProperties() const { return m_properties; }
    inline void SetProperties(const ShaderProperties &properties) { m_properties = properties; }

    inline MaterialFaceCull SetOverrideCullMode() const { return m_override_cull; }
    inline void SetOverrideCullMode(MaterialFaceCull cull_mode) { m_override_cull = cull_mode; }

    inline DeclaredUniforms &GetUniforms() { return m_uniforms; }
    inline const DeclaredUniforms &GetUniforms() const { return m_uniforms; }

    template <class T>
    inline void SetUniform(DeclaredUniform::Id_t id, const T &value)
        { m_uniforms.Set(id, Uniform(value)); uniform_changed = true; }

    template <class T>
    inline void SetUniform(UniformBuffer::Id_t buffer_id, DeclaredUniform::Id_t uniform_id, const T &value)
        { m_uniforms.Set(buffer_id, uniform_id, Uniform(value)); uniform_changed = true; }


    void Use();
    void End();

protected:
    ShaderProperties m_properties;
    MaterialFaceCull m_override_cull;

    enum SubShaderType {
        SUBSHADER_NONE = 0x00,
        SUBSHADER_FRAGMENT = 0x8B30,
        SUBSHADER_VERTEX = 0x8B31,
        SUBSHADER_GEOMETRY = 0x8DD9,
        SUBSHADER_COMPUTE = 0x91B9
    };

    struct SubShader {
        SubShaderType type;
        int id;
        std::string code;
        std::string processed_code;
        std::string path;

        SubShader()
            : type(SubShaderType::SUBSHADER_NONE),
              id(0)
        {
        }

        SubShader(SubShaderType type, const std::string &code)
            : type(type),
              id(0),
              code(code)
        {
        }

        SubShader(const SubShader &other)
            : type(other.type),
              id(other.id),
              code(other.code),
              processed_code(other.processed_code),
              path(other.path)
        {
        }
    };

    void AddSubShader(SubShaderType type,
        const std::string &code,
        const ShaderProperties &properties,
        const std::string &path);
    void PreprocessSubShader(SubShader &sub_shader, const ShaderProperties &properties);

    void InitUniforms();
    void ResetUniforms();

    // has to be called manually in ApplyMaterial() by shaders that base off
    // of this class.
    void SetLightUniforms(Environment *);

    DeclaredUniform::Id_t m_uniform_flip_uv_x;
    DeclaredUniform::Id_t m_uniform_flip_uv_y;
    DeclaredUniform::Id_t m_uniform_uv_scale;
    DeclaredUniform::Id_t m_uniform_viewport;
    // matrices
    DeclaredUniform::Id_t m_uniform_model_matrix;
    DeclaredUniform::Id_t m_uniform_view_matrix;
    DeclaredUniform::Id_t m_uniform_proj_matrix;
    DeclaredUniform::Id_t m_uniform_view_proj_matrix;
    // lights
    DirectionalLightUniform m_uniform_directional_light;
    std::vector<PointLightUniform> m_uniform_point_lights;
    DeclaredUniform::Id_t m_uniform_num_point_lights;

    // textures
    DeclaredUniform::Id_t m_uniform_textures[MATERIAL_MAX_TEXTURES],
        m_uniform_has_textures[MATERIAL_MAX_TEXTURES];

    DeclaredUniforms m_uniforms;

private:
    bool is_uploaded, is_created, uniform_changed;
    unsigned int progid;

    HashCode::Value_t m_previous_properties_hash_code;

    void CreateGpuData();
    void UploadGpuData();
    void DestroyGpuData();
    bool ShaderPropertiesChanged() const;
    void ApplyUniforms();

    UniformBuffer::Internal *CreateUniformBuffer(const char *name);
    void DestroyUniformBuffer(UniformBuffer::Internal *);
    std::vector<UniformBuffer::Internal*> m_uniform_buffer_internals;

    std::map<SubShaderType, SubShader> subshaders;
};

} // namespace hyperion

#endif
