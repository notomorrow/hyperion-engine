#ifndef MATERIAL_H
#define MATERIAL_H

#include "../hash_code.h"
#include "texture.h"
#include "../math/vector2.h"
#include "../math/vector3.h"
#include "../math/vector4.h"
#include "../asset/fbom/fbom.h"
#include "../util/enum_options.h"

#include <string>
#include <array>
#include <map>
#include <memory>

#define MATERIAL_MAX_PARAMETERS 32
#define MATERIAL_MAX_TEXTURES 32

namespace hyperion {

struct MaterialTextureWrapper {
    MaterialTextureWrapper() : m_texture(nullptr) {}
    MaterialTextureWrapper(const std::shared_ptr<Texture> &texture) : m_texture(texture) {}
    MaterialTextureWrapper(const MaterialTextureWrapper &other) : m_texture(other.m_texture) {}

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        if (m_texture != nullptr) {
            hc.Add(int32_t(m_texture->GetTextureType()));
            hc.Add(int32_t(m_texture->GetWidth()));
            hc.Add(int32_t(m_texture->GetHeight()));
            // TODO other
            hc.Add(intptr_t(m_texture->GetBytes())); // pointer to memory of image
        }

        return hc;
    }

    std::shared_ptr<Texture> m_texture;
};

enum MaterialParameterKey : uint64_t {
    MATERIAL_PARAMETER_NONE = 0b00,
    
    // basic
    MATERIAL_PARAMETER_DIFFUSE_COLOR   = 0b10,
    MATERIAL_PARAMETER_METALNESS       = 0b100,
    MATERIAL_PARAMETER_ROUGHNESS       = 0b1000,
    MATERIAL_PARAMETER_SUBSURFACE      = 0b10000,
    MATERIAL_PARAMETER_SPECULAR        = 0b100000,
    MATERIAL_PARAMETER_SPECULAR_TINT   = 0b1000000,
    MATERIAL_PARAMETER_ANISOTROPIC     = 0b10000000,
    MATERIAL_PARAMETER_SHEEN           = 0b100000000,
    MATERIAL_PARAMETER_SHEEN_TINT      = 0b1000000000,
    MATERIAL_PARAMETER_CLEARCOAT       = 0b10000000000,
    MATERIAL_PARAMETER_CLEARCOAT_GLOSS = 0b100000000000,
    MATERIAL_PARAMETER_EMISSIVENESS    = 0b1000000000000,
    MATERIAL_PARAMETER_FLIP_UV         = 0b10000000000000,
    MATERIAL_PARAMETER_UV_SCALE        = 0b100000000000000,
    MATERIAL_PARAMETER_PARALLAX_HEIGHT = 0b1000000000000000,
    MATERIAL_PARAMETER_RESERVED1       = 0b10000000000000000,
    MATERIAL_PARAMETER_RESERVED2       = 0b100000000000000000,
    MATERIAL_PARAMETER_RESERVED3       = 0b1000000000000000000,

    // terrain
    MATERIAL_PARAMETER_TERRAIN_LEVEL_0_HEIGHT = 0b10000000000000000000,
    MATERIAL_PARAMETER_TERRAIN_LEVEL_1_HEIGHT = 0b100000000000000000000,
    MATERIAL_PARAMETER_TERRAIN_LEVEL_2_HEIGHT = 0b1000000000000000000000,
    MATERIAL_PARAMETER_TERRAIN_LEVEL_3_HEIGHT = 0b10000000000000000000000
};

enum MaterialTextureKey : uint64_t {
    MATERIAL_TEXTURE_NONE = 0b00,

    MATERIAL_TEXTURE_DIFFUSE_MAP = 0b01,
    MATERIAL_TEXTURE_NORMAL_MAP = 0b10,
    MATERIAL_TEXTURE_AO_MAP = 0b100,
    MATERIAL_TEXTURE_PARALLAX_MAP = 0b1000,
    MATERIAL_TEXTURE_METALNESS_MAP = 0b10000,
    MATERIAL_TEXTURE_ROUGHNESS_MAP = 0b100000,
    MATERIAL_TEXTURE_SKYBOX_MAP = 0b1000000,
    MATERIAL_TEXTURE_COLOR_MAP = 0b10000000,
    MATERIAL_TEXTURE_POSITION_MAP = 0b100000000,
    MATERIAL_TEXTURE_DATA_MAP = 0b1000000000,
    MATERIAL_TEXTURE_SSAO_MAP = 0b10000000000,
    MATERIAL_TEXTURE_TANGENT_MAP = 0b100000000000,
    MATERIAL_TEXTURE_BITANGENT_MAP = 0b1000000000000,
    MATERIAL_TEXTURE_DEPTH_MAP = 0b10000000000000,

    // terrain

    MATERIAL_TEXTURE_SPLAT_MAP = 0b100000000000000,

    MATERIAL_TEXTURE_BASE_TERRAIN_COLOR_MAP    = 0b1000000000000000,
    MATERIAL_TEXTURE_BASE_TERRAIN_NORMAL_MAP   = 0b10000000000000000,
    MATERIAL_TEXTURE_BASE_TERRAIN_AO_MAP       = 0b100000000000000000,
    MATERIAL_TEXTURE_BASE_TERRAIN_PARALLAX_MAP = 0b1000000000000000000,

    MATERIAL_TEXTURE_TERRAIN_LEVEL1_COLOR_MAP    = 0b10000000000000000000,
    MATERIAL_TEXTURE_TERRAIN_LEVEL1_NORMAL_MAP   = 0b100000000000000000000,
    MATERIAL_TEXTURE_TERRAIN_LEVEL1_AO_MAP       = 0b1000000000000000000000,
    MATERIAL_TEXTURE_TERRAIN_LEVEL1_PARALLAX_MAP = 0b10000000000000000000000,

    MATERIAL_TEXTURE_TERRAIN_LEVEL2_COLOR_MAP    = 0b100000000000000000000000,
    MATERIAL_TEXTURE_TERRAIN_LEVEL2_NORMAL_MAP   = 0b1000000000000000000000000,
    MATERIAL_TEXTURE_TERRAIN_LEVEL2_AO_MAP       = 0b10000000000000000000000000,
    MATERIAL_TEXTURE_TERRAIN_LEVEL2_PARALLAX_MAP = 0b100000000000000000000000000
};

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
    MaterialParameter(float value);
    MaterialParameter(float x, float y);
    MaterialParameter(float x, float y, float z);
    MaterialParameter(float x, float y, float z, float w);
    MaterialParameter(const Vector2 &);
    MaterialParameter(const Vector3 &);
    MaterialParameter(const Vector4 &);
    MaterialParameter(const float *data, size_t nvalues, MaterialParameterType paramtype);
    MaterialParameter(const MaterialParameter &other);
    ~MaterialParameter() = default;

    inline MaterialParameterType GetType() const { return MaterialParameterType(type); }

    inline float &operator[](size_t idx) { return values[idx]; }
    inline float operator[](size_t idx) const { return values[idx]; }
    inline bool operator==(const MaterialParameter &other) const
    {
        return values == other.values
            && type == other.type;
    }
    inline bool operator!=(const MaterialParameter &other) const { return !operator==(other); }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(int(type));

        for (int i = 0; i < values.size(); i++) {
            hc.Add(values[i]);
        }

        return hc;
    }

private:
    int32_t type;
    std::array<float, 4> values;
};

class Material : public fbom::FBOMLoadable {
public:
    using MaterialParameterTable_t = EnumOptions<MaterialParameterKey, MaterialParameter, MATERIAL_MAX_PARAMETERS>;
    using MaterialTextureTable_t = EnumOptions<MaterialTextureKey, MaterialTextureWrapper, MATERIAL_MAX_TEXTURES>;

    static const MaterialParameterTable_t default_parameters;
    static const EnumOptions<MaterialTextureKey, const char *, MATERIAL_MAX_TEXTURES> material_texture_names;

    Material();
    Material(const Material &other);
    virtual ~Material() = default;

    inline bool operator==(const Material &other) const
        { return GetHashCode().Value() == other.GetHashCode().Value(); }

    inline MaterialParameterTable_t &GetParameters() { return m_params; }
    inline const MaterialParameter &GetParameter(MaterialParameterKey key) const { return m_params.Get(key); }

    inline MaterialTextureTable_t &GetTextures() { return m_textures; }
    inline const MaterialTextureTable_t &GetTextures() const { return m_textures; }
    void SetTexture(MaterialTextureKey, const std::shared_ptr<Texture> &);
    std::shared_ptr<Texture> GetTexture(MaterialTextureKey) const;

    void SetParameter(MaterialParameterKey, float);
    void SetParameter(MaterialParameterKey, int);
    void SetParameter(MaterialParameterKey, const Vector2 &);
    void SetParameter(MaterialParameterKey, const Vector3 &);
    void SetParameter(MaterialParameterKey, const Vector4 &);

    MaterialFaceCull cull_faces = MaterialFace_Back;

    bool alpha_blended = false;
    bool depth_test = true;
    bool depth_write = true;
    Vector4 diffuse_color = Vector4(1.0);

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_params.GetHashCode());
        hc.Add(m_textures.GetHashCode());

        hc.Add(alpha_blended);
        hc.Add(depth_test);
        hc.Add(depth_write);
        hc.Add(diffuse_color.GetHashCode());
        hc.Add(int(cull_faces));

        return hc;
    }

    virtual std::shared_ptr<Loadable> Clone() override;


#pragma region serialization
    FBOM_DEF_DESERIALIZER(loader, in, out) {
        using namespace fbom;

        static_assert(sizeof(MaterialParameter) == 4 + 16); /* sizeof(int32_t) + sizeof(float) * 4 */
        static_assert(sizeof(MaterialParameterTable_t) == (sizeof(MaterialParameter) * MATERIAL_MAX_PARAMETERS));

        auto out_material = std::make_shared<Material>();
        out = out_material;

        if (auto err = in->GetProperty("diffuse_color").ReadArrayElements(FBOMFloat(), 4, (unsigned char *)&out_material->diffuse_color)) {
            return err;
        }

        if (auto err = in->GetProperty("parameters").ReadStruct(sizeof(MaterialParameterTable_t), (unsigned char *)&out_material->m_params)) {
            return err;
        }

        return FBOMResult::FBOM_OK;
    }

    FBOM_DEF_SERIALIZER(loader, in, out)
    {
        using namespace fbom;

        auto material = dynamic_cast<Material*>(in);

        if (material == nullptr) {
            return FBOMResult::FBOM_ERR;
        }
 
        out->SetProperty("diffuse_color", FBOMArray(FBOMFloat(), 4), (void*)&material->diffuse_color);
        out->SetProperty("parameters", FBOMStruct(sizeof(MaterialParameterTable_t)), (void*)&material->m_params);

        return FBOMResult::FBOM_OK;
    }
#pragma endregion serialization

private:
    MaterialParameterTable_t m_params;
    MaterialTextureTable_t m_textures;

    std::shared_ptr<Material> CloneImpl();
};

} // namespace hyperion

#endif
