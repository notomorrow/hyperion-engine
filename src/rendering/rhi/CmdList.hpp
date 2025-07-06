/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/MemoryPool.hpp>

#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/Variant.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>

#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderComputePipeline.hpp>
#include <rendering/rt/RenderRaytracingPipeline.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderFramebuffer.hpp>
#include <rendering/RenderImage.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderCommandBuffer.hpp>
#include <rendering/RenderObject.hpp>

// #define HYP_RHI_COMMAND_STACK_TRACE

#ifdef HYP_RHI_COMMAND_STACK_TRACE
#include <core/debug/StackDump.hpp>
#endif

namespace hyperion {

class CmdBase;

class CmdMemoryPoolBase
{
public:
    virtual ~CmdMemoryPoolBase() = default;

    virtual void FreeCommand(CmdBase* command) = 0;
};

struct CmdPoolHandle
{
    CmdMemoryPoolBase* pool;
    uint32 index;
};

class CmdBase
{
public:
    friend class CmdList;

    template <class T>
    friend class CmdMemoryPool;

    CmdBase() = default;
    CmdBase(const CmdBase& other) = delete;
    CmdBase& operator=(const CmdBase& other) = delete;
    CmdBase(CmdBase&& other) noexcept = delete;
    CmdBase& operator=(CmdBase&& other) noexcept = delete;
    virtual ~CmdBase() = default;

    virtual void Prepare(FrameBase* frame)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) = 0;

private:
    CmdPoolHandle m_poolHandle;

#ifdef HYP_RHI_COMMAND_STACK_TRACE
    char* m_stackTrace = "";
#endif
};

template <class CmdType>
class CmdMemoryPool final : public MemoryPool<ValueStorage<CmdType>>, CmdMemoryPoolBase
{
public:
    static CmdMemoryPool<CmdType>& GetInstance()
    {
        static CmdMemoryPool<CmdType> instance;

        return instance;
    }

    virtual ~CmdMemoryPool() override = default;

    template <class... Args>
    CmdType* NewCommand(Args&&... args)
    {
        ValueStorage<CmdType>* commandStorage;

        const uint32 index = MemoryPool<ValueStorage<CmdType>>::AcquireIndex(&commandStorage);

        commandStorage->Construct(std::forward<Args>(args)...);
        commandStorage->Get().m_poolHandle = { static_cast<CmdMemoryPoolBase*>(this), index };

#ifdef HYP_RHI_COMMAND_STACK_TRACE
        // Allocate string and copy stack trace (creating stack trace is extremely SLOW, enable for debugging only!)
        String dump = StackDump(10, 2).ToString();
        commandStorage->Get().m_stackTrace = (char*)std::malloc(dump.Size() + 1);
        std::strncpy(commandStorage->Get().m_stackTrace, dump.Data(), dump.Size());
        commandStorage->Get().m_stackTrace[dump.Size()] = '\0';
#endif

        return &commandStorage->Get();
    }

    virtual void FreeCommand(CmdBase* command) override
    {
        AssertDebug(command != nullptr);
        AssertDebug(command->m_poolHandle.pool == this);

#ifdef HYP_RHI_COMMAND_STACK_TRACE
        if (command->m_stackTrace != nullptr)
        {
            std::free(command->m_stackTrace);
            command->m_stackTrace = nullptr;
        }
#endif

        command->~CmdBase();

        MemoryPool<ValueStorage<CmdType>>::ReleaseIndex(command->m_poolHandle.index);
    }
};

class BindVertexBuffer final : public CmdBase
{
public:
    BindVertexBuffer(const GpuBufferRef& buffer)
        : m_buffer(buffer)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        cmd->BindVertexBuffer(m_buffer);
    }

private:
    GpuBufferRef m_buffer;
};

class BindIndexBuffer final : public CmdBase
{
public:
    BindIndexBuffer(const GpuBufferRef& buffer)
        : m_buffer(buffer)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        cmd->BindIndexBuffer(m_buffer);
    }

private:
    GpuBufferRef m_buffer;
};

class DrawIndexed final : public CmdBase
{
public:
    DrawIndexed(uint32 numIndices, uint32 numInstance, uint32 instanceIndex)
        : m_numIndices(numIndices),
          m_numInstances(numInstance),
          m_instanceIndex(instanceIndex)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        cmd->DrawIndexed(m_numIndices, m_numInstances, m_instanceIndex);
    }

private:
    uint32 m_numIndices;
    uint32 m_numInstances;
    uint32 m_instanceIndex;
};

class DrawIndexedIndirect final : public CmdBase
{
public:
    DrawIndexedIndirect(const GpuBufferRef& buffer, uint32 bufferOffset)
        : m_buffer(buffer),
          m_bufferOffset(bufferOffset)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        cmd->DrawIndexedIndirect(m_buffer, m_bufferOffset);
    }

private:
    GpuBufferRef m_buffer;
    uint32 m_bufferOffset;
};

class BeginFramebuffer final : public CmdBase
{
public:
#ifdef HYP_DEBUG_MODE
    HYP_API BeginFramebuffer(const FramebufferRef& framebuffer, uint32 frameIndex);
    HYP_API virtual void Prepare(FrameBase* frame) override;
#else
    BeginFramebuffer(const FramebufferRef& framebuffer, uint32 frameIndex)
        : m_framebuffer(framebuffer),
          m_frameIndex(frameIndex)
    {
    }
#endif

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_framebuffer->BeginCapture(cmd, m_frameIndex);
    }

private:
    FramebufferRef m_framebuffer;
    uint32 m_frameIndex;
};

class EndFramebuffer final : public CmdBase
{
public:
#ifdef HYP_DEBUG_MODE
    HYP_API EndFramebuffer(const FramebufferRef& framebuffer, uint32 frameIndex);
    HYP_API virtual void Prepare(FrameBase* frame) override;
#else
    EndFramebuffer(const FramebufferRef& framebuffer, uint32 frameIndex)
        : m_framebuffer(framebuffer),
          m_frameIndex(frameIndex)
    {
    }
#endif

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_framebuffer->EndCapture(cmd, m_frameIndex);
    }

private:
    FramebufferRef m_framebuffer;
    uint32 m_frameIndex;
};

class BindGraphicsPipeline final : public CmdBase
{
public:
#ifdef HYP_DEBUG_MODE
    HYP_API BindGraphicsPipeline(const GraphicsPipelineRef& pipeline, Vec2i viewportOffset, Vec2u viewportExtent);
    HYP_API BindGraphicsPipeline(const GraphicsPipelineRef& pipeline);
#else
    BindGraphicsPipeline(const GraphicsPipelineRef& pipeline, Vec2i viewportOffset, Vec2u viewportExtent)
        : m_pipeline(pipeline),
          m_viewportOffset(viewportOffset),
          m_viewportExtent(viewportExtent)
    {
    }

    BindGraphicsPipeline(const GraphicsPipelineRef& pipeline)
        : m_pipeline(pipeline)
    {
    }
#endif

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        if (m_viewportOffset != Vec2i(0, 0) || m_viewportExtent != Vec2u(0, 0))
        {
            m_pipeline->Bind(cmd, m_viewportOffset, m_viewportExtent);
        }
        else
        {
            m_pipeline->Bind(cmd);
        }
    }

private:
    GraphicsPipelineRef m_pipeline;
    Vec2i m_viewportOffset;
    Vec2u m_viewportExtent;
};

class BindComputePipeline final : public CmdBase
{
public:
    BindComputePipeline(const ComputePipelineRef& pipeline)
        : m_pipeline(pipeline)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_pipeline->Bind(cmd);
    }

private:
    ComputePipelineRef m_pipeline;
};

class BindRaytracingPipeline final : public CmdBase
{
public:
    BindRaytracingPipeline(const RaytracingPipelineRef& pipeline)
        : m_pipeline(pipeline)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_pipeline->Bind(cmd);
    }

private:
    RaytracingPipelineRef m_pipeline;
};

class BindDescriptorSet final : public CmdBase
{
public:
    BindDescriptorSet(const DescriptorSetRef& descriptorSet, const GraphicsPipelineRef& pipeline, const ArrayMap<Name, uint32>& offsets = {})
        : m_descriptorSet(descriptorSet),
          m_pipeline(pipeline),
          m_offsets(offsets)
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

        m_bindIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
    }

    BindDescriptorSet(const DescriptorSetRef& descriptorSet, const GraphicsPipelineRef& pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bindIndex)
        : m_descriptorSet(descriptorSet),
          m_pipeline(pipeline),
          m_offsets(offsets),
          m_bindIndex(bindIndex)
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
    }

    BindDescriptorSet(const DescriptorSetRef& descriptorSet, const ComputePipelineRef& pipeline, const ArrayMap<Name, uint32>& offsets = {})
        : m_descriptorSet(descriptorSet),
          m_pipeline(pipeline),
          m_offsets(offsets)
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

        m_bindIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
    }

    BindDescriptorSet(const DescriptorSetRef& descriptorSet, const ComputePipelineRef& pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bindIndex)
        : m_descriptorSet(descriptorSet),
          m_pipeline(pipeline),
          m_offsets(offsets),
          m_bindIndex(bindIndex)
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
    }

    BindDescriptorSet(const DescriptorSetRef& descriptorSet, const RaytracingPipelineRef& pipeline, const ArrayMap<Name, uint32>& offsets = {})
        : m_descriptorSet(descriptorSet),
          m_pipeline(pipeline),
          m_offsets(offsets)
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");

        m_bindIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptorSet->GetLayout().GetName());
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index for descriptor set {}", descriptorSet->GetLayout().GetName());
    }

    BindDescriptorSet(const DescriptorSetRef& descriptorSet, const RaytracingPipelineRef& pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bindIndex)
        : m_descriptorSet(descriptorSet),
          m_pipeline(pipeline),
          m_offsets(offsets),
          m_bindIndex(bindIndex)
    {
        AssertDebug(descriptorSet != nullptr, "Descriptor set must not be null");
        AssertDebug(descriptorSet->IsCreated(), "Descriptor set is not created yet");
        AssertDebug(m_bindIndex != ~0u, "Invalid bind index");
    }

    HYP_API virtual void Prepare(FrameBase* frame) override;

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_pipeline.Visit([this, &cmd](const auto& pipeline)
            {
                m_descriptorSet->Bind(cmd, pipeline, m_offsets, m_bindIndex);
            });
    }

private:
    DescriptorSetRef m_descriptorSet;
    Variant<GraphicsPipelineRef, ComputePipelineRef, RaytracingPipelineRef> m_pipeline;
    ArrayMap<Name, uint32> m_offsets;
    uint32 m_bindIndex;
};

class BindDescriptorTable final : public CmdBase
{
public:
    BindDescriptorTable(const DescriptorTableRef& descriptorTable, const GraphicsPipelineRef& graphicsPipeline, const ArrayMap<Name, ArrayMap<Name, uint32>>& offsets, uint32 frameIndex)
        : m_descriptorTable(descriptorTable),
          m_pipeline(graphicsPipeline),
          m_offsets(offsets),
          m_frameIndex(frameIndex)
    {
        AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
    }

    BindDescriptorTable(const DescriptorTableRef& descriptorTable, const ComputePipelineRef& computePipeline, const ArrayMap<Name, ArrayMap<Name, uint32>>& offsets, uint32 frameIndex)
        : m_descriptorTable(descriptorTable),
          m_pipeline(computePipeline),
          m_offsets(offsets),
          m_frameIndex(frameIndex)
    {
        AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
    }

    BindDescriptorTable(const DescriptorTableRef& descriptorTable, const RaytracingPipelineRef& raytracingPipeline, const ArrayMap<Name, ArrayMap<Name, uint32>>& offsets, uint32 frameIndex)
        : m_descriptorTable(descriptorTable),
          m_pipeline(raytracingPipeline),
          m_offsets(offsets),
          m_frameIndex(frameIndex)
    {
        AssertDebug(descriptorTable != nullptr, "Descriptor table must not be null");
    }

    HYP_API virtual void Prepare(FrameBase* frame) override;

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_pipeline.Visit([this, &cmd](const auto& pipeline)
            {
                m_descriptorTable->Bind(cmd, m_frameIndex, pipeline, m_offsets);
            });
    }

private:
    DescriptorTableRef m_descriptorTable;
    Variant<GraphicsPipelineRef, ComputePipelineRef, RaytracingPipelineRef> m_pipeline;
    ArrayMap<Name, ArrayMap<Name, uint32>> m_offsets;
    uint32 m_frameIndex;
};

class InsertBarrier final : public CmdBase
{
public:
    InsertBarrier(const GpuBufferRef& buffer, const ResourceState& state, ShaderModuleType shaderModuleType = SMT_UNSET)
        : m_buffer(buffer),
          m_state(state),
          m_shaderModuleType(shaderModuleType)
    {
    }

    InsertBarrier(const ImageRef& image, const ResourceState& state, ShaderModuleType shaderModuleType = SMT_UNSET)
        : m_image(image),
          m_state(state),
          m_shaderModuleType(shaderModuleType)
    {
    }

    InsertBarrier(const ImageRef& image, const ResourceState& state, const ImageSubResource& subResource, ShaderModuleType shaderModuleType = SMT_UNSET)
        : m_image(image),
          m_state(state),
          m_subResource(subResource),
          m_shaderModuleType(shaderModuleType)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        if (m_buffer)
        {
            m_buffer->InsertBarrier(cmd, m_state, m_shaderModuleType);
        }
        else if (m_image)
        {
            if (m_subResource)
            {
                m_image->InsertBarrier(cmd, *m_subResource, m_state, m_shaderModuleType);
            }
            else
            {
                m_image->InsertBarrier(cmd, m_state, m_shaderModuleType);
            }
        }
    }

private:
    GpuBufferRef m_buffer;
    ImageRef m_image;
    ResourceState m_state;
    Optional<ImageSubResource> m_subResource;
    ShaderModuleType m_shaderModuleType;
};

class Blit final : public CmdBase
{
public:
    Blit(const ImageRef& srcImage, const ImageRef& dstImage)
        : m_srcImage(srcImage),
          m_dstImage(dstImage)
    {
    }

    Blit(const ImageRef& srcImage, const ImageRef& dstImage, const Rect<uint32>& srcRect, const Rect<uint32>& dstRect)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_srcRect(srcRect),
          m_dstRect(dstRect)
    {
    }

    Blit(const ImageRef& srcImage, const ImageRef& dstImage, const Rect<uint32>& srcRect, const Rect<uint32>& dstRect, uint32 srcMip, uint32 dstMip, uint32 srcFace, uint32 dstFace)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_srcRect(srcRect),
          m_dstRect(dstRect),
          m_mipFaceInfo(MipFaceInfo { srcMip, dstMip, srcFace, dstFace })
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        if (m_mipFaceInfo)
        {
            MipFaceInfo info = *m_mipFaceInfo;

            if (m_srcRect && m_dstRect)
            {
                m_dstImage->Blit(cmd, m_srcImage, *m_srcRect, *m_dstRect, info.srcMip, info.dstMip, info.srcFace, info.dstFace);
            }
            else
            {
                m_dstImage->Blit(cmd, m_srcImage, info.srcMip, info.dstMip, info.srcFace, info.dstFace);
            }
        }
        else
        {
            if (m_srcRect && m_dstRect)
            {
                m_dstImage->Blit(cmd, m_srcImage, *m_srcRect, *m_dstRect);
            }
            else
            {
                m_dstImage->Blit(cmd, m_srcImage);
            }
        }
    }

private:
    struct MipFaceInfo
    {
        uint32 srcMip;
        uint32 dstMip;
        uint32 srcFace;
        uint32 dstFace;
    };

    ImageRef m_srcImage;
    ImageRef m_dstImage;

    Optional<Rect<uint32>> m_srcRect;
    Optional<Rect<uint32>> m_dstRect;

    Optional<MipFaceInfo> m_mipFaceInfo;
};

class BlitRect final : public CmdBase
{
public:
    BlitRect(const ImageRef& srcImage, const ImageRef& dstImage, const Rect<uint32>& srcRect, const Rect<uint32>& dstRect)
        : m_srcImage(srcImage),
          m_dstImage(dstImage),
          m_srcRect(srcRect),
          m_dstRect(dstRect)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_dstImage->Blit(cmd, m_srcImage, m_srcRect, m_dstRect);
    }

private:
    ImageRef m_srcImage;
    ImageRef m_dstImage;
    Rect<uint32> m_srcRect;
    Rect<uint32> m_dstRect;
};

class CopyImageToBuffer final : public CmdBase
{
public:
    CopyImageToBuffer(const ImageRef& image, const GpuBufferRef& buffer)
        : m_image(image),
          m_buffer(buffer)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_image->CopyToBuffer(cmd, m_buffer);
    }

private:
    ImageRef m_image;
    GpuBufferRef m_buffer;
};

class CopyBufferToImage final : public CmdBase
{
public:
    CopyBufferToImage(const GpuBufferRef& buffer, const ImageRef& image)
        : m_buffer(buffer),
          m_image(image)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_image->CopyFromBuffer(cmd, m_buffer);
    }

private:
    GpuBufferRef m_buffer;
    ImageRef m_image;
};

class CopyBuffer final : public CmdBase
{
public:
    CopyBuffer(const GpuBufferRef& srcBuffer, const GpuBufferRef& dstBuffer, SizeType size)
        : m_srcBuffer(srcBuffer),
          m_dstBuffer(dstBuffer),
          m_size(size)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_dstBuffer->CopyFrom(cmd, m_srcBuffer, m_size);
    }

private:
    GpuBufferRef m_srcBuffer;
    GpuBufferRef m_dstBuffer;
    SizeType m_size;
};

class GenerateMipmaps final : public CmdBase
{
public:
    GenerateMipmaps(const ImageRef& image)
        : m_image(image)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_image->GenerateMipmaps(cmd);
    }

private:
    ImageRef m_image;
};

class DispatchCompute final : public CmdBase
{
public:
    DispatchCompute(const ComputePipelineRef& pipeline, Vec3u workgroupCount)
        : m_pipeline(pipeline),
          m_workgroupCount(workgroupCount)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_pipeline->Dispatch(cmd, m_workgroupCount);
    }

private:
    ComputePipelineRef m_pipeline;
    Vec3u m_workgroupCount;
};

class TraceRays final : public CmdBase
{
public:
    TraceRays(const RaytracingPipelineRef& pipeline, const Vec3u& workgroupCount)
        : m_pipeline(pipeline),
          m_workgroupCount(workgroupCount)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_pipeline->TraceRays(cmd, m_workgroupCount);
    }

private:
    RaytracingPipelineRef m_pipeline;
    Vec3u m_workgroupCount;
};

class CmdList
{
public:
    CmdList();
    CmdList(const CmdList& other) = delete;
    CmdList& operator=(const CmdList& other) = delete;
    CmdList(CmdList&& other) noexcept = delete;
    CmdList& operator=(CmdList&& other) noexcept = delete;
    ~CmdList();

    HYP_FORCE_INLINE const Array<CmdBase*>& GetCommands() const
    {
        return m_commands;
    }

    template <class CmdType, class... Args>
    void Add(Args&&... args)
    {
        static CmdMemoryPool<CmdType>& pool = CmdMemoryPool<CmdType>::GetInstance();

        m_commands.PushBack(pool.NewCommand(std::forward<Args>(args)...));
    }

    void Concat(CmdList&& other)
    {
        m_commands.Concat(std::move(other.m_commands));
    }

    void Prepare(FrameBase* frame);
    void Execute(const CommandBufferRef& cmd);

private:
    void FreeCommand(CmdBase* command);

    Array<CmdBase*> m_commands;
};

} // namespace hyperion
