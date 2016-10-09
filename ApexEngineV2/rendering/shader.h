#ifndef SHADER_H
#define SHADER_H

#include "../opengl.h"
#include "camera/camera.h"
#include "../math/vector2.h"
#include "../math/vector3.h"
#include "../math/vector4.h"
#include "../math/matrix4.h"
#include "../asset/loadable.h"
#include "material.h"

#include <vector>
#include <array>
#include <map>
#include <string>

namespace apex {
typedef std::map<std::string, float> ShaderProperties;

class Shader {
public:
    Shader(const ShaderProperties &properties);
    Shader(const ShaderProperties &properties,
        const std::string &vscode, const std::string &fscode);
    ~Shader();

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Matrix4 &transform, Camera *camera);

    inline void SetUniform(const std::string &name, float value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, int value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, const Vector2 &value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, const Vector3 &value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, const Vector4 &value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, const Matrix4 &value) { uniforms[name] = Uniform(value); uniform_changed = true; }

    void Use();
    void End();

protected:
    struct SubShader {
        int type;
        int id;
        std::string code;

        SubShader()
            : type(0),
              id(0)
        {
        }

        SubShader(int type, const std::string &code)
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

    inline void AddSubShader(const SubShader &sub_shader) { subshaders.push_back(sub_shader); }

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

    std::vector<SubShader> subshaders;
    std::map<std::string, Uniform> uniforms;
};
} // namespace apex

#endif