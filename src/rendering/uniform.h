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

    Uniform(const Uniform &other)
        : type(other.type)
    {
        memcpy(&data, &other.data, sizeof(other.data));
    }

    explicit Uniform(float value)
    {
        data.f = value;
        type = UniformType::UNIFORM_TYPE_FLOAT;
    }

    explicit Uniform(int value)
    {
        data.i32 = value;
        type = UniformType::UNIFORM_TYPE_I32;
    }

    explicit Uniform(const Vector2 &value)
    {
        data.vec2[0] = value.x;
        data.vec2[1] = value.y;
        type = UniformType::UNIFORM_TYPE_VEC2;
    }

    explicit Uniform(const Vector3 &value)
    {
        data.vec3[0] = value.x;
        data.vec3[1] = value.y;
        data.vec3[2] = value.z;
        type = UniformType::UNIFORM_TYPE_VEC3;
    }

    explicit Uniform(const Vector4 &value)
    {
        data.vec4[0] = value.x;
        data.vec4[1] = value.y;
        data.vec4[2] = value.z;
        data.vec4[3] = value.w;
        type = UniformType::UNIFORM_TYPE_VEC4;
    }

    explicit Uniform(const Matrix4 &value)
    {
        std::memcpy(&data.mat4[0], &value.values[0], value.values.size() * sizeof(float));
        type = UniformType::UNIFORM_TYPE_MAT4;
    }

    explicit Uniform(const Texture *texture)
    {
        if (texture != nullptr) {
            data.texture_id = texture->GetId();
            // texture->GetTextureType() should start at 0 and map to the correct uniform texture type
            type = UniformType(int(Uniform::UniformType::UNIFORM_TYPE_TEXTURE2D) + int(texture->GetTextureType()));
        } else {
            data.texture_id = 0;
            type = Uniform::UniformType::UNIFORM_TYPE_TEXTURE2D;
        }
    }

    inline Uniform &operator=(const Uniform &other)
    {
        type = other.type;
        memcpy(&data, &other.data, sizeof(other.data));
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
    }

    inline bool operator!=(const Uniform &other) const { return !operator==(other); }

    inline bool IsTextureType() const
    {
        return type == Uniform::UniformType::UNIFORM_TYPE_TEXTURE2D
            || type == Uniform::UniformType::UNIFORM_TYPE_TEXTURE3D
            || type == Uniform::UniformType::UNIFORM_TYPE_TEXTURECUBE;
    }

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


struct DeclaredUniform {
    using Id_t = int;

    Id_t id;
    std::string name;
    Uniform value;

    DeclaredUniform(Id_t id, const std::string &name, Uniform value = Uniform())
        : id(id), name(name), value(value)
    {
    }

    DeclaredUniform(const DeclaredUniform &other)
        : id(other.id), name(other.name), value(other.value)
    {
    }

    inline DeclaredUniform &operator=(const DeclaredUniform &other)
    {
        id = other.id;
        name = other.name;
        value = other.value;

        return *this;
    }
};

struct UniformResult;
struct UniformBufferResult;

class DeclaredUniforms {
public:

    DeclaredUniforms() { m_uniforms.reserve(32); }
    DeclaredUniforms(const DeclaredUniforms &other) = delete;
    inline DeclaredUniforms &operator=(const DeclaredUniforms &other) = delete;

    UniformBufferResult AcquireBuffer(const std::string &name)
    {
        UniformBuffer::Id_t id = m_uniform_buffers.size();
        m_uniform_buffers.push_back(std::make_pair(UniformBuffer(id, name), true));

        return UniformBufferResult(UniformBufferResult::DECLARED_UNIFORM_BUFFER_OK, id);
    }

    UniformResult Acquire(const std::string &name)
    {
        DeclaredUniform::Id_t id = m_uniforms.size();
        m_uniforms.push_back(std::make_pair(DeclaredUniform(id, name), true));

        return UniformResult(UniformResult::DECLARED_UNIFORM_OK, id);
    }

    UniformResult Acquire(const std::string &name, const Uniform &initial_value)
    {
        DeclaredUniform::Id_t id = m_uniforms.size();
        m_uniforms.push_back(std::make_pair(DeclaredUniform(id, name, initial_value), true));

        return UniformResult(UniformResult::DECLARED_UNIFORM_OK, id);
    }

    UniformResult Acquire(UniformBuffer::Id_t buffer_id, const std::string &name, const Uniform &initial_value)
    {
        AssertThrow(buffer_id >= 0);
        AssertThrow(buffer_id < m_uniform_buffers.size());

        auto &buffer = m_uniform_buffers[buffer_id].first;

        return buffer.Acquire(name, initial_value);
    }

    inline bool Set(DeclaredUniform::Id_t id, const Uniform &uniform)
    {
        AssertThrow(id >= 0);
        AssertThrow(id < m_uniforms.size());

        if (m_uniforms[id].first.value != uniform || uniform.IsTextureType()) {
            m_uniforms[id].first.value = uniform;
            m_uniforms[id].second = true;

            return true;
        }

        return false;
    }

    inline bool Set(UniformBuffer::Id_t buffer_id, DeclaredUniform::Id_t uniform_id, const Uniform &uniform)
    {
        AssertThrow(buffer_id >= 0);
        AssertThrow(buffer_id < m_uniform_buffers.size());

        auto &buffer = m_uniform_buffers[buffer_id].first;

        AssertThrow(uniform_id >= 0);
        AssertThrow(uniform_id < buffer.data.size());

        bool changed = buffer.Set(uniform_id, uniform);

        m_uniform_buffers[buffer_id].second |= changed;

        return changed;
    }

    // bool - has changed?
    std::vector<std::pair<DeclaredUniform, bool>> m_uniforms;
    std::vector<std::pair<UniformBuffer, bool>> m_uniform_buffers;
};


struct UniformResult {
    enum {
        DECLARED_UNIFORM_OK,
        DECLARED_UNIFORM_ERR
    } result;

    DeclaredUniform::Id_t id;

    std::string message;

    UniformResult(decltype(result) result, DeclaredUniform::Id_t id = -1, const std::string &message = "")
        : result(result), message(message), id(id) {}
    UniformResult(const UniformResult &other)
        : result(other.result), message(other.message), id(other.id) {}
    inline UniformResult &operator=(const UniformResult &other)
    {
        result = other.result;
        id = other.id;
        message = other.message;

        return *this;
    }

    inline explicit operator bool() const { return result == DECLARED_UNIFORM_OK; }
};

struct UniformBufferResult {
    enum {
        DECLARED_UNIFORM_BUFFER_OK,
        DECLARED_UNIFORM_BUFFER_ERR
    } result;

    UniformBuffer::Id_t id;

    std::string message;

    UniformBufferResult(decltype(result) result, UniformBuffer::Id_t id = -1, const std::string &message = "")
        : result(result), message(message), id(id) {}
    UniformBufferResult(const UniformBufferResult &other)
        : result(other.result), message(other.message), id(other.id) {}
    inline UniformBufferResult &operator=(const UniformBufferResult &other)
    {
        result = other.result;
        id = other.id;
        message = other.message;

        return *this;
    }

    inline explicit operator bool() const { return result == DECLARED_UNIFORM_BUFFER_OK; }
};

} // namespace hyperion

#endif