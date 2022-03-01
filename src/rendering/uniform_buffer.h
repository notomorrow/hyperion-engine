#ifndef UNIFORM_BUFFER_H
#define UNIFORM_BUFFER_H

#include "uniform.h"

#include <string>
#include <vector>

namespace hyperion {

class Shader;

struct UniformBuffer {
    using Id_t = int;

    Id_t id;
    std::string name;
    std::vector<DeclaredUniform> data;

    struct Internal {
        using Handle_t = unsigned int;

        Handle_t handle;
        size_t size;
        size_t index;
        bool generated;

        Internal() = default;
        Internal(const Internal &other) = delete;
        Internal &operator=(const Internal &other) = delete;
    };

    non_owning_ptr<const Internal> _internal;

    UniformBuffer(Id_t id, const std::string &name)
        : id(id),
        name(name),
        _internal(nullptr)
    {
    }

    UniformBuffer(const UniformBuffer &other)
        : id(other.id),
        name(other.name),
        data(other.data),
        _internal(other._internal)
    {
    }

    inline UniformBuffer &operator=(const UniformBuffer &other)
    {
        id = other.id;
        name = other.name;
        data = other.data;
        _internal = other._internal;

        return *this;
    }

    UniformResult Acquire(const std::string &name, const Uniform &initial_value)
    {
        DeclaredUniform::Id_t id = data.size();
        data.push_back(DeclaredUniform(id, name, initial_value));

        return UniformResult(UniformResult::DECLARED_UNIFORM_OK, id);
    }

    inline bool Set(DeclaredUniform::Id_t id, const Uniform &uniform)
    {
        AssertThrow(id >= 0);
        AssertThrow(id < data.size());


        if (data[id].value != uniform || uniform.IsTextureType()) {
            data[id].value = uniform;

            return true;
        }

        return false;
    }
};

class UniformBufferInternalsHolder {
public:
    UniformBufferInternalsHolder();
    UniformBufferInternalsHolder(const UniformBufferInternalsHolder &other) = delete;
    UniformBufferInternalsHolder &operator=(const UniformBufferInternalsHolder &other) = delete;
    ~UniformBufferInternalsHolder();

    UniformBuffer::Internal *CreateUniformBufferInternal(Shader *shader, UniformBuffer &);
    void DestroyUniformBufferInternal(UniformBuffer::Internal *);
    void Reset();

    std::vector<UniformBuffer::Internal *> m_internals;
};

} // namespace hyperion

#endif