#ifndef HYPERION_V2_MATERIAL_H
#define HYPERION_V2_MATERIAL_H

#include "texture.h"

#include <util/enum_options.h>
#include <hash_code.h>

namespace hyperion::v2 {

class TextureSet {
public:
    static constexpr size_t max_textures = 32;

    enum TextureKey : uint64_t {
        MATERIAL_TEXTURE_NONE = 0,

        MATERIAL_TEXTURE_DIFFUSE_MAP = 1 << 0,
        MATERIAL_TEXTURE_NORMAL_MAP = 1 << 1,
        MATERIAL_TEXTURE_AO_MAP = 1 << 2,
        MATERIAL_TEXTURE_PARALLAX_MAP = 1 << 3,
        MATERIAL_TEXTURE_METALNESS_MAP = 1 << 4,
        MATERIAL_TEXTURE_ROUGHNESS_MAP = 1 << 5,
        MATERIAL_TEXTURE_SKYBOX_MAP = 1 << 6,
        MATERIAL_TEXTURE_COLOR_MAP = 1 << 7,
        MATERIAL_TEXTURE_POSITION_MAP = 1 << 8,
        MATERIAL_TEXTURE_DATA_MAP = 1 << 9,
        MATERIAL_TEXTURE_SSAO_MAP = 1 << 10,
        MATERIAL_TEXTURE_TANGENT_MAP = 1 << 11,
        MATERIAL_TEXTURE_BITANGENT_MAP = 1 << 12,
        MATERIAL_TEXTURE_DEPTH_MAP = 1 << 13,

        // terrain

        MATERIAL_TEXTURE_SPLAT_MAP = 1 << 14,

        MATERIAL_TEXTURE_BASE_TERRAIN_COLOR_MAP = 1 << 15,
        MATERIAL_TEXTURE_BASE_TERRAIN_NORMAL_MAP = 1 << 16,
        MATERIAL_TEXTURE_BASE_TERRAIN_AO_MAP = 1 << 17,
        MATERIAL_TEXTURE_BASE_TERRAIN_PARALLAX_MAP = 1 << 18,

        MATERIAL_TEXTURE_TERRAIN_LEVEL1_COLOR_MAP = 1 << 19,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_NORMAL_MAP = 1 << 20,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_AO_MAP = 1 << 21,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_PARALLAX_MAP = 1 << 22,

        MATERIAL_TEXTURE_TERRAIN_LEVEL2_COLOR_MAP = 1 << 23,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_NORMAL_MAP = 1 << 24,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_AO_MAP = 1 << 25,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_PARALLAX_MAP = 1 << 26
    };


    EnumOptions<TextureKey, Texture::ID, max_textures> m_textures;
};

class Material {
public:
    static constexpr size_t max_parameters = 32;

    struct Parameter {
        union {
            float float_values[4];
            int int_values[4];
        } values;

        enum Type {
            MATERIAL_PARAMETER_TYPE_NONE,
            MATERIAL_PARAMETER_TYPE_FLOAT_1,
            MATERIAL_PARAMETER_TYPE_FLOAT_2,
            MATERIAL_PARAMETER_TYPE_FLOAT_3,
            MATERIAL_PARAMETER_TYPE_FLOAT_4,
            MATERIAL_PARAMETER_TYPE_INT_1,
            MATERIAL_PARAMETER_TYPE_INT_2,
            MATERIAL_PARAMETER_TYPE_INT_3,
            MATERIAL_PARAMETER_TYPE_INT_4
        } type;

        Parameter() : type(MATERIAL_PARAMETER_TYPE_NONE) {}

        Parameter(const Parameter &other)
            : type(other.type)
        {
            std::memcpy(&values, &other.values, sizeof(values));
        }

        Parameter &operator=(const Parameter &other)
        {
            type = other.type;
            std::memcpy(&values, &other.values, sizeof(values));

            return *this;
        }

        ~Parameter() = default;

        inline uint8_t Size() const
        {
            if (type == MATERIAL_PARAMETER_TYPE_NONE) {
                return uint8_t(0);
            }

            if (type >= MATERIAL_PARAMETER_TYPE_INT_1) {
                return uint8_t(type - MATERIAL_PARAMETER_TYPE_INT_1) + 1;
            }

            return uint8_t(type);
        }

        inline void Copy(unsigned char dst[4]) const
        {
            std::memcpy(dst, &values, Size());
        }
    };

    enum State {
        MATERIAL_STATE_CLEAN,
        MATERIAL_STATE_DIRTY
    };
    
    enum MaterialKey : uint64_t {
        MATERIAL_KEY_NONE = 0,

        // basic
        MATERIAL_KEY_ALBEDO                 = 1 << 0,
        MATERIAL_KEY_METALNESS              = 1 << 1,
        MATERIAL_KEY_ROUGHNESS              = 1 << 2,
        MATERIAL_KEY_SUBSURFACE             = 1 << 3,
        MATERIAL_KEY_SPECULAR               = 1 << 4,
        MATERIAL_KEY_SPECULAR_TINT          = 1 << 5,
        MATERIAL_KEY_ANISOTROPIC            = 1 << 6,
        MATERIAL_KEY_SHEEN                  = 1 << 7,
        MATERIAL_KEY_SHEEN_TINT             = 1 << 8,
        MATERIAL_KEY_CLEARCOAT              = 1 << 9,
        MATERIAL_KEY_CLEARCOAT_GLOSS        = 1 << 10,
        MATERIAL_KEY_EMISSIVENESS           = 1 << 11,
        MATERIAL_KEY_FLIP_UV                = 1 << 12,
        MATERIAL_KEY_UV_SCALE               = 1 << 13,
        MATERIAL_KEY_PARALLAX_HEIGHT        = 1 << 14,
        MATERIAL_KEY_RESERVED1              = 1 << 15,
        MATERIAL_KEY_RESERVED2              = 1 << 16,
        MATERIAL_KEY_RESERVED3              = 1 << 17,

        // terrain
        MATERIAL_KEY_TERRAIN_LEVEL_0_HEIGHT = 1 << 18,
        MATERIAL_KEY_TERRAIN_LEVEL_1_HEIGHT = 1 << 19,
        MATERIAL_KEY_TERRAIN_LEVEL_2_HEIGHT = 1 << 20,
        MATERIAL_KEY_TERRAIN_LEVEL_3_HEIGHT = 1 << 21
    };

    Material();
    Material(const Material &other) = delete;
    Material &operator=(const Material &other) = delete;
    ~Material();
    
    void SetParameter(MaterialKey key, const Parameter &value);
    void SetTexture(TextureKey key, Texture::ID id);

private:
    State m_state;

    EnumOptions<MaterialKey, Parameter, max_parameters> m_parameters;
};

} // namespace hyperion::v2

#endif