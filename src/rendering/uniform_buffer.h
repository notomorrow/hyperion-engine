#ifndef UNIFORM_BUFFER_H
#define UNIFORM_BUFFER_H

#include "uniform.h"
#include "declared_uniform.h"
#include "../util/non_owning_ptr.h"

#include "vulkan/renderer_buffer.h"
#include "vulkan/renderer_device.h"

#include <vulkan/vulkan.h>

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
        using Handle_t = VkBuffer;

        Handle_t handle;
        size_t size;
        size_t index;
        bool generated;
        RendererGPUBuffer *gpu_buffer;

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

    inline size_t TotalSize() const
    {
        size_t size = 0;

        for (const auto &item : data) {
            size += item.value.GetSize();
        }

        return size;
    }
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

class UniformBufferInternalsHolder {
public:
    UniformBufferInternalsHolder(RendererDevice *);
    UniformBufferInternalsHolder(const UniformBufferInternalsHolder &other) = delete;
    UniformBufferInternalsHolder &operator=(const UniformBufferInternalsHolder &other) = delete;
    ~UniformBufferInternalsHolder();

    UniformBuffer::Internal *CreateUniformBufferInternal(Shader *shader, UniformBuffer &);
    void DestroyUniformBufferInternal(UniformBuffer::Internal *);
    void Reset();

    std::vector<UniformBuffer::Internal *> m_internals;
    non_owning_ptr<RendererDevice> _device;
};

} // namespace hyperion

#endif