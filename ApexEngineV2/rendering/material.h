#ifndef MATERIAL_H
#define MATERIAL_H

#include "texture.h"
#include "../math/vector2.h"
#include "../math/vector3.h"
#include "../math/vector4.h"

#include <string>
#include <array>
#include <map>
#include <memory>

namespace apex {
class MaterialParameter {
public:
    enum MaterialParameterType {
        MATPARAM_NONE,
        MATPARAM_FLOAT,
        MATPARAM_INT,
        MATPARAM_TEXHANDLE,
        MATPARAM_VEC2,
        MATPARAM_VEC3,
        MATPARAM_VEC4
    };

    MaterialParameter();
    MaterialParameter(const float *data, size_t nvalues, MaterialParameterType paramtype);
    MaterialParameter(const MaterialParameter &other);

    bool IsEmpty() const;
    size_t NumValues() const;
    MaterialParameterType GetType() const;
    
    float &operator[](size_t idx);
    float operator[](size_t idx) const;

private:
    size_t size;
    std::array<float, 8> values;
    MaterialParameterType type;
};

class Material {
public:
    Material();
    Material(const Material &other);

    bool HasParameter(const std::string &name) const;
    
    std::map<std::string, MaterialParameter> &GetParameters();
    const MaterialParameter &GetParameter(const std::string &name) const;
    
    void SetParameter(const std::string &name, float);
    void SetParameter(const std::string &name, int);
    void SetParameter(const std::string &name, const std::shared_ptr<Texture> &);
    void SetParameter(const std::string &name, const Vector2 &);
    void SetParameter(const std::string &name, const Vector3 &);
    void SetParameter(const std::string &name, const Vector4 &);

    bool alpha_blended = false;
    bool depth_test = true;
    bool depth_write = true;
    Vector4 diffuse_color = Vector4(1.0);
    std::shared_ptr<Texture> diffuse_texture = nullptr;
    std::shared_ptr<Texture> normals_texture = nullptr;

private:
    std::map<std::string, MaterialParameter> params;
};
}

#endif