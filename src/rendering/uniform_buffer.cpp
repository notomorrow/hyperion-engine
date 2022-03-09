#include "uniform_buffer.h"
#include "shader.h"
#include "../gl_util.h"
#include "../opengl.h"

namespace hyperion {

UniformBufferInternalsHolder::UniformBufferInternalsHolder(renderer::Device *device) : _device(device) {}
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

    unsigned char *raw_uniform_data = new unsigned char[total_size]; // TODO: more linear layout for the above data table to ease copying
    // OR add an offset parameter to Copy() to facilitate doing this in a loop
    size_t offset = 0;
    for (size_t i = 0; i < uniform_buffer.data.size(); i++) {
        size_t sz = uniform_buffer.data[i].value.GetSize();
        std::memcpy(raw_uniform_data + offset, uniform_buffer.data[i].value.GetRawPtr(), sz);
        offset += sz;
    }

    UniformBuffer::Internal *_internal = new UniformBuffer::Internal;
    _internal->generated = false;

    _internal->gpu_buffer = new renderer::UniformBuffer();
    _internal->gpu_buffer->Create(_device.get(), total_size);
    _internal->gpu_buffer->Copy(_device.get(), total_size, raw_uniform_data);

    delete[] raw_uniform_data;

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
        _internal->gpu_buffer->Destroy(_device.get());
        delete _internal->gpu_buffer;
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