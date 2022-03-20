#ifndef HYPERION_V2_MATERIAL_H
#define HYPERION_V2_MATERIAL_H

#include <util/enum_options.h>
#include <hash_code.h>

namespace hyperion::v2 {

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

        Parameter(const Parameter &other) = delete;
        Parameter &operator=(const Parameter &other) = delete;

        Parameter(Parameter &&other)
            : type(other.type)
        {
            other.type = MATERIAL_PARAMETER_TYPE_NONE;
            std::memcpy(&values, &other.values, sizeof(values));
        }

        Parameter &operator=(Parameter &&other)
        {
            type = other.type;
            other.type = MATERIAL_PARAMETER_TYPE_NONE;
            std::memcpy(&values, &other.values, sizeof(values));
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

private:
    EnumOptions<MaterialKey, Parameter, max_parameters> m_parameters;
};

} // namespace hyperion::v2

#endif