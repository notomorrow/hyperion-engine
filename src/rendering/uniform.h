#ifndef UNIFORM_H
#define UNIFORM_H

#include <array>
#include <cstring>

#include "../math/vector2.h"
#include "../math/vector3.h"
#include "../math/vector4.h"
#include "../math/matrix4.h"
#include "texture.h"
#include "../util.h"

namespace hyperion {

class Shader;

class Uniform {
public:

    enum class UniformType {
        UNIFORM_TYPE_NONE = 0,
        UNIFORM_TYPE_FLOAT,
        UNIFORM_TYPE_I32,
        UNIFORM_TYPE_I64,
        UNIFORM_TYPE_U32,
        UNIFORM_TYPE_U64,
        UNIFORM_TYPE_VEC2,
        UNIFORM_TYPE_VEC3,
        UNIFORM_TYPE_VEC4,
        UNIFORM_TYPE_MAT4,
        UNIFORM_TYPE_TEXTURE2D,
        UNIFORM_TYPE_TEXTURE3D,
        UNIFORM_TYPE_TEXTURECUBE
    } type;

    union {
        void *v;
        float f;
        int32_t i32;
        int64_t i64;
        uint32_t u32;
        uint64_t u64;
        float vec2[2];
        float vec3[3];
        float vec4[4];
        float mat4[16];
        Texture::Id_t texture_id;
    } data;

    Uniform()
    {
        data.v = 0;
        type = UniformType::UNIFORM_TYPE_NONE;
    }

    Uniform(float value)
    {
        data.f = value;
        type = UniformType::UNIFORM_TYPE_FLOAT;
    }

    Uniform(int value)
    {
        data.i32 = value;
        type = UniformType::UNIFORM_TYPE_I32;
    }

    Uniform(const Vector2 &value)
    {
        data.vec2[0] = value.x;
        data.vec2[1] = value.y;
        type = UniformType::UNIFORM_TYPE_VEC2;
    }

    Uniform(const Vector3 &value)
    {
        data.vec3[0] = value.x;
        data.vec3[1] = value.y;
        data.vec3[2] = value.z;
        type = UniformType::UNIFORM_TYPE_VEC3;
    }

    Uniform(const Vector4 &value)
    {
        data.vec4[0] = value.x;
        data.vec4[1] = value.y;
        data.vec4[2] = value.z;
        data.vec4[3] = value.w;
        type = UniformType::UNIFORM_TYPE_VEC4;
    }

    Uniform(const Matrix4 &value)
    {
        std::memcpy(&data.mat4[0], &value.values[0], value.values.size() * sizeof(float));
        type = UniformType::UNIFORM_TYPE_MAT4;
    }

    Uniform(const Texture *texture)
    {
        ex_assert(texture != nullptr);

        data.texture_id = texture->GetId();
        // texture->GetTextureType() should start at 0 and map to the correct uniform texture type
        type = UniformType(int(Uniform::UniformType::UNIFORM_TYPE_TEXTURE2D) + int(texture->GetTextureType()));
    }

    inline Uniform &operator=(const Uniform &other)
    {
        type = other.type;
        data = other.data;
        return *this;
    }

    inline bool operator==(const Uniform &other) const
    {
        if (type != other.type) {
            return false;
        }

        if (type == Uniform::UniformType::UNIFORM_TYPE_NONE) {
            return true;
        }

        return !memcmp(GetRawPtr(), other.GetRawPtr(), GetSize());

        switch (type) {
        case Uniform::UniformType::UNIFORM_TYPE_FLOAT: return data.f == other.data.f;
        case Uniform::UniformType::UNIFORM_TYPE_I32: return data.i32 == other.data.i32;
        case Uniform::UniformType::UNIFORM_TYPE_I64: return data.i64 == other.data.i64;
        case Uniform::UniformType::UNIFORM_TYPE_U32: return data.u32 == other.data.u32;
        case Uniform::UniformType::UNIFORM_TYPE_U64: return data.u64 == other.data.u64;
        case Uniform::UniformType::UNIFORM_TYPE_VEC2: return !memcmp(&data.vec2[0], &other.data.vec2[0], sizeof(float) * 2);
        case Uniform::UniformType::UNIFORM_TYPE_VEC3: return !memcmp(&data.vec3[0], &other.data.vec3[0], sizeof(float) * 3);
        case Uniform::UniformType::UNIFORM_TYPE_VEC4: return !memcmp(&data.vec4[0], &other.data.vec4[0], sizeof(float) * 4);
        case Uniform::UniformType::UNIFORM_TYPE_MAT4: return !memcmp(&data.mat4[0], &other.data.mat4[0], sizeof(float) * 16);
        case Uniform::UniformType::UNIFORM_TYPE_TEXTURE2D:
            // fallthrough
        case Uniform::UniformType::UNIFORM_TYPE_TEXTURE3D:
            // fallthrough
        case Uniform::UniformType::UNIFORM_TYPE_TEXTURECUBE:
            return data.texture_id == other.data.texture_id;
        default:
            return true;
        }
    }

    inline bool operator!=(const Uniform &other) const { return !operator==(other); }

    inline void *GetRawPtr() const
    {
        switch (type) {
        case Uniform::UniformType::UNIFORM_TYPE_FLOAT: return (void *)&data.f;
        case Uniform::UniformType::UNIFORM_TYPE_I32: return (void *)&data.i32;
        case Uniform::UniformType::UNIFORM_TYPE_I64: return (void *)&data.i64;
        case Uniform::UniformType::UNIFORM_TYPE_U32: return (void *)&data.u32;
        case Uniform::UniformType::UNIFORM_TYPE_U64: return (void *)&data.u64;
        case Uniform::UniformType::UNIFORM_TYPE_VEC2: return (void *)&data.vec2[0];
        case Uniform::UniformType::UNIFORM_TYPE_VEC3: return (void *)&data.vec3[0];
        case Uniform::UniformType::UNIFORM_TYPE_VEC4: return (void *)&data.vec4[0];
        case Uniform::UniformType::UNIFORM_TYPE_MAT4: return (void *)&data.mat4[0];
        case Uniform::UniformType::UNIFORM_TYPE_TEXTURE2D:
            // fallthrough
        case Uniform::UniformType::UNIFORM_TYPE_TEXTURE3D:
            // fallthrough
        case Uniform::UniformType::UNIFORM_TYPE_TEXTURECUBE:
            return (void *)&data.texture_id;
        default:
            return 0;
        }
    }

    inline size_t GetSize() const
    {
        switch (type) {
        case Uniform::UniformType::UNIFORM_TYPE_FLOAT: return sizeof(data.f);
        case Uniform::UniformType::UNIFORM_TYPE_I32: return sizeof(data.i32);
        case Uniform::UniformType::UNIFORM_TYPE_I64: return sizeof(data.i64);
        case Uniform::UniformType::UNIFORM_TYPE_U32: return sizeof(data.u32);
        case Uniform::UniformType::UNIFORM_TYPE_U64: return sizeof(data.u64);
        case Uniform::UniformType::UNIFORM_TYPE_VEC2: return sizeof(data.vec2);
        case Uniform::UniformType::UNIFORM_TYPE_VEC3: return sizeof(data.vec3);
        case Uniform::UniformType::UNIFORM_TYPE_VEC4: return sizeof(data.vec4);
        case Uniform::UniformType::UNIFORM_TYPE_MAT4: return sizeof(data.mat4);
        case Uniform::UniformType::UNIFORM_TYPE_TEXTURE2D:
            // fallthrough
        case Uniform::UniformType::UNIFORM_TYPE_TEXTURE3D:
            // fallthrough
        case Uniform::UniformType::UNIFORM_TYPE_TEXTURECUBE:
            return sizeof(data.texture_id);
        default:
            return 0;
        }
    }

    void BindUniform(Shader *shader, const char *name, int &texture_index);
};
} // namespace hyperion

#endif