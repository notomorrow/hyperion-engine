#ifndef SHADER_H
#define SHADER_H

#include "../core_engine.h"
#include "../math/vector2.h"
#include "../math/vector3.h"
#include "../math/vector4.h"
#include "../math/matrix4.h"
#include "../asset/loadable.h"
#include "material.h"

#include <vector>
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
    virtual void ApplyTransforms(const Matrix4 &model, const Matrix4 &view, const Matrix4 &proj);

    void SetUniform(const std::string &name, float);
    void SetUniform(const std::string &name, int);
    void SetUniform(const std::string &name, const Vector2 &);
    void SetUniform(const std::string &name, const Vector3 &);
    void SetUniform(const std::string &name, const Vector4 &);
    void SetUniform(const std::string &name, const Matrix4 &);

    void Use();
    void End();

protected:
    struct SubShader {
        int type;
        std::string code;
        int id;

        SubShader()
            : type(0), code(""), id(0)
        {
        }

        SubShader(int type, const std::string &code)
            : type(type), code(code), id(0)
        {
        }

        SubShader(const SubShader &other)
            : type(other.type), code(other.code), id(other.id)
        {
        }
    };

    void AddSubShader(const SubShader &);

private:
    bool is_uploaded, is_created, uniform_changed;
    unsigned int progid;

    struct Uniform {
        enum {
            UF_NONE = -1,
            UF_FLOAT,
            UF_INT,
            UF_VEC2,
            UF_VEC3,
            UF_VEC4,
            UF_MAT4
        } type;

        float data[16];

        Uniform()
        {
            std::memset(data, 0, sizeof(data));
            type = UF_NONE;
        }

        Uniform(float value)
        {
            data[0] = value;
            type = UF_FLOAT;
        }

        Uniform(int value)
        {
            data[0] = value;
            type = UF_INT;
        }

        Uniform(const Vector2 &value)
        {
            data[0] = value.x;
            data[1] = value.y;
            type = UF_VEC2;
        }

        Uniform(const Vector3 &value)
        {
            data[0] = value.x;
            data[1] = value.y;
            data[2] = value.z;
            type = UF_VEC3;
        }

        Uniform(const Vector4 &value)
        {
            data[0] = value.x;
            data[1] = value.y;
            data[2] = value.z;
            data[3] = value.w;
            type = UF_VEC4;
        }

        Uniform(const Matrix4 &value)
        {
            std::memcpy(data, value.values, sizeof(data));
            type = UF_MAT4;
        }

        Uniform &operator=(const Uniform &other)
        {
            type = other.type;
            std::memcpy(data, other.data, sizeof(data));
            return *this;
        }
    };

    std::vector<SubShader> subshaders;
    std::map<std::string, Uniform> uniforms;
};
}

#endif