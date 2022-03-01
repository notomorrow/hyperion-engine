#include "uniform_buffer.h"
#include "shader.h"
#include "../gl_util.h"
#include "../opengl.h"

namespace hyperion {

UniformBufferInternalsHolder::UniformBufferInternalsHolder() {}
UniformBufferInternalsHolder::~UniformBufferInternalsHolder()
{
    Reset();
}

UniformBuffer::Internal *UniformBufferInternalsHolder::CreateUniformBufferInternal(Shader *shader, UniformBuffer &uniform_buffer)
{
    size_t total_size = 0; // calculate total size of data allocated for uniform buffer
    for (size_t i = 0; i < uniform_buffer.data.size(); i++) {
        total_size += uniform_buffer.data[i].value.GetSize();
    }

    UniformBuffer::Internal *_internal = new UniformBuffer::Internal;
    _internal->generated = false;

    // TODO: assert total_size fits into block_size?
    GLuint block_index = glGetUniformBlockIndex(shader->GetId(), uniform_buffer.name.c_str());
    CatchGLErrors("Failed to get uniform block index");

    if (block_index != GL_INVALID_INDEX) {
        GLint block_size;
        glGetActiveUniformBlockiv(shader->GetId(), block_index, GL_UNIFORM_BLOCK_DATA_SIZE, &block_size);
        CatchGLErrors("Failed to get active uniform block size");

        _internal->size = block_size;

        GLuint handle;
        glGenBuffers(1, &handle);
        CatchGLErrors("Failed to generate uniform buffer");

        glBindBuffer(GL_UNIFORM_BUFFER, handle);
        glBufferData(GL_UNIFORM_BUFFER, total_size, nullptr, GL_STATIC_DRAW);
        CatchGLErrors("Failed to set uniform buffer initial data");
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        _internal->handle = handle;
        _internal->index = block_index;
        _internal->generated = true;
    }


    m_internals.push_back(_internal);

    return _internal;
}

void UniformBufferInternalsHolder::DestroyUniformBufferInternal(UniformBuffer::Internal *_internal)
{
    auto it = std::find(
        m_internals.begin(),
        m_internals.end(),
        _internal
    );

    AssertThrow(it != m_internals.end());

    if (_internal->generated) {
        GLuint handle = _internal->handle;
        glDeleteBuffers(1, &handle);
        CatchGLErrors("Failed to delete uniform buffer");
    }

    delete _internal;

    m_internals.erase(it);
}

void UniformBufferInternalsHolder::Reset()
{
    for (auto *uniform_buffer : m_internals) {
        DestroyUniformBufferInternal(uniform_buffer);
    }

    m_internals.clear();
}

} // namespace hyperion