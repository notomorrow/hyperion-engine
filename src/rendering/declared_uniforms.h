#ifndef DECLARED_UNIFORMS_H
#define DECLARED_UNIFORMS_H

#include "uniform.h"
#include "declared_uniform.h"

namespace hyperion {

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

} // namespace hyperion

#endif