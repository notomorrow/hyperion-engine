#ifndef SHADER_H
#define SHADER_H

#include "../math/vector2.h"
#include "../math/vector3.h"
#include "../math/vector4.h"
#include "../math/matrix4.h"
#include "../math/transform.h"
#include "../asset/loadable.h"
#include "material.h"
#include "camera/camera.h"

#include <vector>
#include <array>
#include <map>
#include <string>
#include <cstring>

namespace apex {

typedef std::map<std::string, float> ShaderProperties;

class Shader {
public:
    Shader(const ShaderProperties &properties);
    Shader(const ShaderProperties &properties,
        const std::string &vscode, const std::string &fscode);
    virtual ~Shader();

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);

    inline ShaderProperties &GetProperties() { return m_properties; }
    inline const ShaderProperties &GetProperties() const { return m_properties; }
    inline MaterialFaceCull SetOverrideCullMode() const { return m_override_cull; }
    inline void SetOverrideCullMode(MaterialFaceCull cull_mode) { m_override_cull = cull_mode; }

    inline void SetUniform(const std::string &name, float value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, int value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, const Vector2 &value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, const Vector3 &value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, const Vector4 &value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, const Matrix4 &value) { uniforms[name] = Uniform(value); uniform_changed = true; }

    void Use();
    void End();

protected:
    ShaderProperties m_properties;
    MaterialFaceCull m_override_cull;

    enum SubShaderType {
        SUBSHADER_NONE = 0x00,
        SUBSHADER_FRAGMENT = 0x8B30,
        SUBSHADER_VERTEX = 0x8B31,
        SUBSHADER_GEOMETRY = 0x8DD9
    };

    struct SubShader {
        SubShaderType type;
        int id;
        std::string code;

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
              code(other.code)
        {
        }
    };

    inline void AddSubShader(const SubShader &sub_shader) { subshaders[sub_shader.type] = sub_shader; }

    void ResetUniforms();

private:
    bool is_uploaded, is_created, uniform_changed;
    unsigned int progid;

    struct Uniform {
        enum {
            Uniform_None = -1,
            Uniform_Float,
            Uniform_Int,
            Uniform_Vector2,
            Uniform_Vector3,
            Uniform_Vector4,
            Uniform_Matrix4
        } type;

        std::array<float, 16> data;

        Uniform()
        {
            data = { 0.0f };
            type = Uniform_None;
        }

        Uniform(float value)
        {
            data[0] = value;
            type = Uniform_Float;
        }

        Uniform(int value)
        {
            data[0] = (float)value;
            type = Uniform_Int;
        }

        Uniform(const Vector2 &value)
        {
            data[0] = value.x;
            data[1] = value.y;
            type = Uniform_Vector2;
        }

        Uniform(const Vector3 &value)
        {
            data[0] = value.x;
            data[1] = value.y;
            data[2] = value.z;
            type = Uniform_Vector3;
        }

        Uniform(const Vector4 &value)
        {
            data[0] = value.x;
            data[1] = value.y;
            data[2] = value.z;
            data[3] = value.w;
            type = Uniform_Vector4;
        }

        Uniform(const Matrix4 &value)
        {
            std::memcpy(&data[0], &value.values[0], value.values.size() * sizeof(float));
            type = Uniform_Matrix4;
        }

        Uniform &operator=(const Uniform &other)
        {
            type = other.type;
            data = other.data;
            return *this;
        }
    };

    std::map<SubShaderType, SubShader> subshaders;
    std::map<std::string, Uniform> uniforms;
};

} // namespace apex

#endif
