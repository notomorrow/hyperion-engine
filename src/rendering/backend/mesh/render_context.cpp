#include "../../mesh.h"
#include "../../vulkan/renderer_buffer.h"

namespace hyperion {
Mesh::RenderContext::RenderContext(Mesh *mesh, VkRenderer *renderer)
    : _mesh(mesh),
      _renderer(renderer),
      _vbo(new RendererGPUBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)),
      _ibo(new RendererGPUBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
{
}

Mesh::RenderContext::~RenderContext()
{
    RendererDevice *device = _renderer->GetRendererDevice();

    _vbo->Destroy(device);
    delete _vbo;

    _ibo->Destroy(device);
    delete _ibo;
}

void Mesh::RenderContext::Create(VkCommandBuffer *cmd)
{
    RendererDevice *device = _renderer->GetRendererDevice();
    VkDevice vk_device = device->GetDevice();
}

void Mesh::RenderContext::Upload(VkCommandBuffer *cmd)
{
    const VkDeviceSize offsets[] = { 0 };

    RendererDevice *device = _renderer->GetRendererDevice();
    VkDevice vk_device = device->GetDevice();

    std::vector<float> buffer = _mesh->CreateBuffer();
    const size_t gpu_buffer_size = buffer.size() * sizeof(float);

    /* Bind and copy vertex buffer */
    _vbo->Create(device, gpu_buffer_size);
    vkCmdBindVertexBuffers(*cmd, 0, 1, &_vbo->buffer, offsets);

    void *memory_buffer = nullptr;
    vkMapMemory(vk_device, _vbo->memory, 0, gpu_buffer_size, 0, &memory_buffer);
    memcpy(memory_buffer, &buffer[0], gpu_buffer_size);
    vkUnmapMemory(vk_device, _vbo->memory);

    for (auto &index : _mesh->indices) {
        DebugLog(LogType::Debug, "Index : %d\n", index);
    }
    /* Bind and copy index buffer */
    size_t gpu_indices_size = _mesh->indices.size() * sizeof(MeshIndex);
    _ibo->Create(device, gpu_indices_size);
    vkCmdBindIndexBuffer(*cmd, _ibo->buffer, 0, VK_INDEX_TYPE_UINT32);

    memory_buffer = nullptr;
    vkMapMemory(vk_device, _ibo->memory, 0, gpu_indices_size, 0, &memory_buffer);
    memcpy(memory_buffer, _mesh->indices.data(), gpu_indices_size);
    vkUnmapMemory(vk_device, _ibo->memory);
}

void Mesh::RenderContext::Draw(VkCommandBuffer *cmd)
{
    const VkDeviceSize offsets[] = { 0 };

    vkCmdBindVertexBuffers(*cmd, 0, 1, &_vbo->buffer, offsets);
    vkCmdBindIndexBuffer(*cmd, _ibo->buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(*cmd, _mesh->indices.size(), 1, 0, 0, 0);

    DebugLog(LogType::Info, "DRAW %d\n", _mesh->indices.size());
}
} // namespace hyperion