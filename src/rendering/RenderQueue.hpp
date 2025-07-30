/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

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

class CmdBase
{
public:
    CmdBase() = default;
    CmdBase(const CmdBase& other) = delete;
    CmdBase& operator=(const CmdBase& other) = delete;
    CmdBase(CmdBase&& other) noexcept = default;
    CmdBase& operator=(CmdBase&& other) noexcept = default;

    static inline void PrepareStatic(CmdBase* cmd, FrameBase* frame)
    {
    }

protected:
    ~CmdBase() = default;
};

class BindVertexBuffer final : public CmdBase
{
public:
    BindVertexBuffer(const GpuBufferRef& buffer)
        : m_buffer(buffer)
    {
        Assert(buffer && buffer->IsCreated());
    }

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        BindVertexBuffer* cmdCasted = static_cast<BindVertexBuffer*>(cmd);

        commandBuffer->BindVertexBuffer(cmdCasted->m_buffer);

        cmdCasted->~BindVertexBuffer();
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
        Assert(buffer && buffer->IsCreated());
    }

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        BindIndexBuffer* cmdCasted = static_cast<BindIndexBuffer*>(cmd);

        commandBuffer->BindIndexBuffer(cmdCasted->m_buffer);

        cmdCasted->~BindIndexBuffer();
    }

private:
    GpuBufferRef m_buffer;
};

class DrawIndexed final : public CmdBase
{
public:
    DrawIndexed(uint32 numIndices, uint32 numInstances = 1, uint32 instanceIndex = 0)
        : m_numIndices(numIndices),
          m_numInstances(numInstances),
          m_instanceIndex(instanceIndex)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        DrawIndexed* cmdCasted = static_cast<DrawIndexed*>(cmd);

        commandBuffer->DrawIndexed(cmdCasted->m_numIndices, cmdCasted->m_numInstances, cmdCasted->m_instanceIndex);

        cmdCasted->~DrawIndexed();
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

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        DrawIndexedIndirect* cmdCasted = static_cast<DrawIndexedIndirect*>(cmd);

        commandBuffer->DrawIndexedIndirect(cmdCasted->m_buffer, cmdCasted->m_bufferOffset);

        cmdCasted->~DrawIndexedIndirect();
    }

private:
    GpuBufferRef m_buffer;
    uint32 m_bufferOffset;
};

class BeginFramebuffer final : public CmdBase
{
public:
#ifdef HYP_DEBUG_MODE
    HYP_API BeginFramebuffer(const FramebufferRef& framebuffer);
    HYP_API static void PrepareStatic(CmdBase* cmd, FrameBase* frame);
#else
    BeginFramebuffer(const FramebufferRef& framebuffer)
        : m_framebuffer(framebuffer)
    {
    }
#endif

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        BeginFramebuffer* cmdCasted = static_cast<BeginFramebuffer*>(cmd);

        cmdCasted->m_framebuffer->BeginCapture(commandBuffer);

        cmdCasted->~BeginFramebuffer();
    }

private:
    FramebufferRef m_framebuffer;
};

class EndFramebuffer final : public CmdBase
{
public:
#ifdef HYP_DEBUG_MODE
    HYP_API EndFramebuffer(const FramebufferRef& framebuffer);
    HYP_API static void PrepareStatic(CmdBase* cmd, FrameBase* frame);
#else
    EndFramebuffer(const FramebufferRef& framebuffer)
        : m_framebuffer(framebuffer)
    {
    }
#endif

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        EndFramebuffer* cmdCasted = static_cast<EndFramebuffer*>(cmd);

        cmdCasted->m_framebuffer->EndCapture(commandBuffer);

        cmdCasted->~EndFramebuffer();
    }

private:
    FramebufferRef m_framebuffer;
};

class ClearFramebuffer final : public CmdBase
{
public:
    HYP_API ClearFramebuffer(const FramebufferRef& framebuffer)
        : m_framebuffer(framebuffer)
    {
    }

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        ClearFramebuffer* cmdCasted = static_cast<ClearFramebuffer*>(cmd);

        cmdCasted->m_framebuffer->Clear(commandBuffer);

        cmdCasted->~ClearFramebuffer();
    }

private:
    FramebufferRef m_framebuffer;
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

    HYP_API static void PrepareStatic(CmdBase* cmd, FrameBase*);

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        BindGraphicsPipeline* cmdCasted = static_cast<BindGraphicsPipeline*>(cmd);

        if (cmdCasted->m_viewportOffset != Vec2i(0, 0) || cmdCasted->m_viewportExtent != Vec2u(0, 0))
        {
            cmdCasted->m_pipeline->Bind(commandBuffer, cmdCasted->m_viewportOffset, cmdCasted->m_viewportExtent);
        }
        else
        {
            cmdCasted->m_pipeline->Bind(commandBuffer);
        }

        cmdCasted->~BindGraphicsPipeline();
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

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        BindComputePipeline* cmdCasted = static_cast<BindComputePipeline*>(cmd);

        cmdCasted->m_pipeline->Bind(commandBuffer);

        cmdCasted->~BindComputePipeline();
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

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        BindRaytracingPipeline* cmdCasted = static_cast<BindRaytracingPipeline*>(cmd);

        cmdCasted->m_pipeline->Bind(commandBuffer);

        cmdCasted->~BindRaytracingPipeline();
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

    HYP_API static void PrepareStatic(CmdBase* cmd, FrameBase* frame);

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        BindDescriptorSet* cmdCasted = static_cast<BindDescriptorSet*>(cmd);

        cmdCasted->m_pipeline.Visit([cmdCasted, &commandBuffer](const auto& pipeline)
            {
                cmdCasted->m_descriptorSet->Bind(commandBuffer, pipeline, cmdCasted->m_offsets, cmdCasted->m_bindIndex);
            });

        cmdCasted->~BindDescriptorSet();
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

    HYP_API static void PrepareStatic(CmdBase* cmd, FrameBase* frame);

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        BindDescriptorTable* cmdCasted = static_cast<BindDescriptorTable*>(cmd);

        cmdCasted->m_pipeline.Visit([cmdCasted, &commandBuffer](const auto& pipeline)
            {
                cmdCasted->m_descriptorTable->Bind(commandBuffer, cmdCasted->m_frameIndex, pipeline, cmdCasted->m_offsets);
            });

        cmdCasted->~BindDescriptorTable();
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

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        InsertBarrier* cmdCasted = static_cast<InsertBarrier*>(cmd);

        if (cmdCasted->m_buffer)
        {
            cmdCasted->m_buffer->InsertBarrier(commandBuffer, cmdCasted->m_state, cmdCasted->m_shaderModuleType);
        }
        else if (cmdCasted->m_image)
        {
            if (cmdCasted->m_subResource)
            {
                cmdCasted->m_image->InsertBarrier(commandBuffer, *cmdCasted->m_subResource, cmdCasted->m_state, cmdCasted->m_shaderModuleType);
            }
            else
            {
                cmdCasted->m_image->InsertBarrier(commandBuffer, cmdCasted->m_state, cmdCasted->m_shaderModuleType);
            }
        }

        cmdCasted->~InsertBarrier();
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

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        Blit* cmdCasted = static_cast<Blit*>(cmd);

        if (cmdCasted->m_mipFaceInfo)
        {
            MipFaceInfo info = *cmdCasted->m_mipFaceInfo;

            if (cmdCasted->m_srcRect && cmdCasted->m_dstRect)
            {
                cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage, *cmdCasted->m_srcRect, *cmdCasted->m_dstRect, info.srcMip, info.dstMip, info.srcFace, info.dstFace);
            }
            else
            {
                cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage, info.srcMip, info.dstMip, info.srcFace, info.dstFace);
            }
        }
        else
        {
            if (cmdCasted->m_srcRect && cmdCasted->m_dstRect)
            {
                cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage, *cmdCasted->m_srcRect, *cmdCasted->m_dstRect);
            }
            else
            {
                cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage);
            }
        }

        cmdCasted->~Blit();
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

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        BlitRect* cmdCasted = static_cast<BlitRect*>(cmd);

        cmdCasted->m_dstImage->Blit(commandBuffer, cmdCasted->m_srcImage, cmdCasted->m_srcRect, cmdCasted->m_dstRect);

        cmdCasted->~BlitRect();
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

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        CopyImageToBuffer* cmdCasted = static_cast<CopyImageToBuffer*>(cmd);

        cmdCasted->m_image->CopyToBuffer(commandBuffer, cmdCasted->m_buffer);

        cmdCasted->~CopyImageToBuffer();
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

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        CopyBufferToImage* cmdCasted = static_cast<CopyBufferToImage*>(cmd);

        cmdCasted->m_image->CopyFromBuffer(commandBuffer, cmdCasted->m_buffer);

        cmdCasted->~CopyBufferToImage();
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

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        CopyBuffer* cmdCasted = static_cast<CopyBuffer*>(cmd);

        cmdCasted->m_dstBuffer->CopyFrom(commandBuffer, cmdCasted->m_srcBuffer, cmdCasted->m_size);

        cmdCasted->~CopyBuffer();
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

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        GenerateMipmaps* cmdCasted = static_cast<GenerateMipmaps*>(cmd);

        cmdCasted->m_image->GenerateMipmaps(commandBuffer);

        cmdCasted->~GenerateMipmaps();
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

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        DispatchCompute* cmdCasted = static_cast<DispatchCompute*>(cmd);

        cmdCasted->m_pipeline->Dispatch(commandBuffer, cmdCasted->m_workgroupCount);

        cmdCasted->~DispatchCompute();
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

    static inline void InvokeStatic(CmdBase* cmd, const CommandBufferRef& commandBuffer)
    {
        TraceRays* cmdCasted = static_cast<TraceRays*>(cmd);

        cmdCasted->m_pipeline->TraceRays(commandBuffer, cmdCasted->m_workgroupCount);

        cmdCasted->~TraceRays();
    }

private:
    RaytracingPipelineRef m_pipeline;
    Vec3u m_workgroupCount;
};

class RenderQueue
{
    using InvokeCmdFnPtr = void (*)(CmdBase*, const CommandBufferRef&);
    using PrepareCmdFnPtr = void (*)(CmdBase*, FrameBase* frame);
    using MoveCmdFnPtr = void (*)(CmdBase*, void* where);

    struct CmdHeader
    {
        uint32 dataOffset;
        InvokeCmdFnPtr invokeFnPtr;
        PrepareCmdFnPtr prepareFnPtr;
        MoveCmdFnPtr moveFnPtr;
    };

    template <class CmdType>
    static inline void MoveCmdStatic(CmdBase* cmd, void* where)
    {
        new (where) CmdType(std::move(*static_cast<CmdType*>(cmd)));
        static_cast<CmdType*>(cmd)->~CmdType();
    }

public:
    RenderQueue();
    RenderQueue(const RenderQueue& other) = delete;
    RenderQueue& operator=(const RenderQueue& other) = delete;
    RenderQueue(RenderQueue&& other) noexcept = delete;
    RenderQueue& operator=(RenderQueue&& other) noexcept = delete;
    ~RenderQueue();

    template <class CmdType>
    void Add(CmdType&& cmd)
    {
        using TCmd = NormalizedType<CmdType>;
        static_assert(alignof(TCmd) <= 16, "CmdType should have alignment <= 16!");

        constexpr SizeType cmdSize = sizeof(TCmd);

        const uint32 alignedOffset = ByteUtil::AlignAs(m_offset, alignof(TCmd));

        if (m_buffer.Size() < alignedOffset + cmdSize)
        {
            ubyte* prevPtr = m_buffer.Data();

            ByteBuffer newBuffer;
            newBuffer.SetSize(2 * (alignedOffset + cmdSize));

            ubyte* newPtr = newBuffer.Data();

            ReconstructCommands(m_cmdHeaders.ToSpan(), prevPtr, newPtr);

            m_buffer = std::move(newBuffer);

            AssertDebug(m_buffer.Data() == newPtr);
        }

        void* startPtr = m_buffer.Data() + alignedOffset;
        new (startPtr) TCmd(std::forward<CmdType>(cmd));

        m_cmdHeaders.PushBack(CmdHeader {
            alignedOffset,
            &TCmd::InvokeStatic,
            &TCmd::PrepareStatic,
            &MoveCmdStatic<TCmd> });

        m_offset = alignedOffset + cmdSize;
    }

    template <class CmdType>
    RenderQueue& operator<<(CmdType&& cmd)
    {
        Add(std::forward<CmdType>(cmd));

        return *this;
    }

    void Concat(RenderQueue&& other)
    {
        m_cmdHeaders.Reserve(m_cmdHeaders.Size() + other.m_cmdHeaders.Size());

        // since we guarantee <= 16 byte alignment, we should just align our offset to 16 to make sure everything fits
        const uint32 newStartOffset = ByteUtil::AlignAs(m_offset, 16);

        if (m_buffer.GetCapacity() < newStartOffset + other.m_offset)
        {
            ubyte* prevPtr = m_buffer.Data();

            ByteBuffer newBuffer;
            newBuffer.SetSize(2 * (newStartOffset + other.m_offset));

            ubyte* newPtr = newBuffer.Data();

            ReconstructCommands(m_cmdHeaders.ToSpan(), prevPtr, newPtr);

            m_buffer = std::move(newBuffer);

            AssertDebug(m_buffer.Data() == newPtr);
        }
        else
        {
            ubyte* prevPtr = m_buffer.Data();

            // No need to reconstruct commands if the allocation did not change
            m_buffer.SetSize(newStartOffset + other.m_offset);

            // Sanity check to ensure SetSize() did not change our capacity. (it shouldn't)
            AssertDebug(m_buffer.Data() == prevPtr);
        }

        SizeType cmdsOffset = m_cmdHeaders.Size();

        // Reconstruct the commands into our memory
        ReconstructCommands(other.m_cmdHeaders.ToSpan(), other.m_buffer.Data(), m_buffer.Data() + newStartOffset);

        // Add headers and update offsets
        for (const CmdHeader& cmdHeader : other.m_cmdHeaders)
        {
            CmdHeader& newCmdHeader = m_cmdHeaders.PushBack(cmdHeader);
            newCmdHeader.dataOffset = newStartOffset + cmdHeader.dataOffset;
        }

        //        // Copy from other buffer, starting at the new offset
        //        m_buffer.Write(other.m_offset, newStartOffset, other.m_buffer.Data());

        m_offset = newStartOffset + other.m_offset;

        // clear out allocation
        other.m_buffer = ByteBuffer();
        other.m_cmdHeaders.Clear();
        other.m_offset = 0;
    }

    void Prepare(FrameBase* frame);
    void Execute(const CommandBufferRef& cmd);

private:
    // Call when buffer is resized to ensure proper move of resources into new memory locations
    static void ReconstructCommands(Span<CmdHeader> cmdHeaders, void* prevPtr, void* newPtr)
    {
        const uintptr_t uPrevPtr = reinterpret_cast<uintptr_t>(prevPtr);
        const uintptr_t uNewPtr = reinterpret_cast<uintptr_t>(newPtr);

        for (CmdHeader& cmdHeader : cmdHeaders)
        {
            cmdHeader.moveFnPtr(reinterpret_cast<CmdBase*>(uPrevPtr + cmdHeader.dataOffset), reinterpret_cast<void*>(uNewPtr + cmdHeader.dataOffset));
        }
    }

    Array<CmdHeader> m_cmdHeaders;
    ByteBuffer m_buffer;
    uint32 m_offset;
};

} // namespace hyperion
