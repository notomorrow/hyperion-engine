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

enum MaterialParameterType {
    MaterialParameter_None,
    MaterialParameter_Float,
    MaterialParameter_Int,
    MaterialParameter_Texture,
    MaterialParameter_Vector2,
    MaterialParameter_Vector3,
    MaterialParameter_Vector4
};

enum MaterialFaceCull {
    MaterialFace_None = 0x00,
    MaterialFace_Front = 0x01,
    MaterialFace_Back = 0x02
};

class MaterialParameter {
public:
    MaterialParameter();
    MaterialParameter(const float value);
    MaterialParameter(const float *data, size_t nvalues, MaterialParameterType paramtype);
    MaterialParameter(const MaterialParameter &other);

    inline bool IsEmpty() const { return size == 0; }
    inline size_t NumValues() const { return size; }
    inline MaterialParameterType GetType() const { return type; }

    inline float &operator[](size_t idx) { return values[idx]; }
    inline float operator[](size_t idx) const { return values[idx]; }

private:
    size_t size;
    std::array<float, 8> values;
    MaterialParameterType type;
};

class Material {
public:
    static const std::map<std::string, MaterialParameter> default_parameters;

    Material();
    Material(const Material &other);

    bool HasParameter(const std::string &name) const;

    std::map<std::string, MaterialParameter> &GetParameters();
    const MaterialParameter &GetParameter(const std::string &name) const;

    void SetParameter(const std::string &name, float);
    void SetParameter(const std::string &name, int);
    void SetParameter(const std::string &name, const Vector2 &);
    void SetParameter(const std::string &name, const Vector3 &);
    void SetParameter(const std::string &name, const Vector4 &);

    void SetTexture(const std::string &name, const std::shared_ptr<Texture> &);
    std::shared_ptr<Texture> GetTexture(const std::string &name) const;

    MaterialFaceCull cull_faces = MaterialFace_Back;

    bool alpha_blended = false;
    bool depth_test = true;
    bool depth_write = true;
    Vector4 diffuse_color = Vector4(1.0);

    std::map<std::string, std::shared_ptr<Texture>> textures;
    std::shared_ptr<Texture> texture0 = nullptr;
    std::shared_ptr<Texture> texture1 = nullptr;
    std::shared_ptr<Texture> texture2 = nullptr;
    std::shared_ptr<Texture> texture3 = nullptr;
    std::shared_ptr<Texture> normals0 = nullptr;
    std::shared_ptr<Texture> normals1 = nullptr;
    std::shared_ptr<Texture> normals2 = nullptr;
    std::shared_ptr<Texture> normals3 = nullptr;

private:
    std::map<std::string, MaterialParameter> params;
};

} // namespace apex

#endif
