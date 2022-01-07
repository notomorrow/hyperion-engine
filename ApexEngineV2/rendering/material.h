#ifndef MATERIAL_H
#define MATERIAL_H

#include "../hash_code.h"
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

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &it : params) {
            hc.Add(it.first);
            
            for (int i = 0; i < it.second.NumValues(); i++) {
                hc.Add(it.second[i]);
            }
        }

        for (const auto &it : textures) {
            if (it.second == nullptr) {
                continue;
            }

            hc.Add(it.first);
            hc.Add(intptr_t(it.second->GetBytes())); // pointer to memory of image
        }

        hc.Add(alpha_blended);
        hc.Add(depth_test);
        hc.Add(depth_write);
        hc.Add(diffuse_color.GetHashCode());
        hc.Add(int(cull_faces));

        return hc;
    }

private:
    std::map<std::string, MaterialParameter> params;
};

} // namespace apex

#endif
