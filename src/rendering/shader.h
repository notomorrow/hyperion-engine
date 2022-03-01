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
#include "uniform_buffer.h"
#include "declared_uniforms.h"
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
        { uniform_changed |= m_uniforms.Set(id, Uniform(value)); }

    template <class T>
    inline void SetUniform(UniformBuffer::Id_t buffer_id, DeclaredUniform::Id_t uniform_id, const T &value)
        { uniform_changed |= m_uniforms.Set(buffer_id, uniform_id, Uniform(value)); }


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
    std::map<SubShaderType, SubShader> subshaders;

    UniformBufferInternalsHolder m_uniform_buffer_internals;

    void CreateGpuData();
    void UploadGpuData();
    void DestroyGpuData();
    bool ShaderPropertiesChanged() const;
    void ApplyUniforms();

    void CreateUniformBufferObjects();
    void DestroyUniformBufferObjects();
};

} // namespace hyperion

#endif
