/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderBackend.hpp>

#include <rendering/CrashHandler.hpp>

#include <core/containers/HashMap.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

class VulkanInstance;

class VulkanDescriptorSetLayoutWrapper;
using VulkanDescriptorSetLayoutWrapperRef = RenderObjectHandle_Strong<VulkanDescriptorSetLayoutWrapper>;
using VulkanDescriptorSetLayoutWrapperWeakRef = RenderObjectHandle_Weak<VulkanDescriptorSetLayoutWrapper>;

class VulkanDescriptorSetManager;

extern HYP_API VkDescriptorSetLayout GetVkDescriptorSetLayout(const VulkanDescriptorSetLayoutWrapper& layout);

class VulkanRenderBackend final : public IRenderBackend
{
public:
    VulkanRenderBackend();
    virtual ~VulkanRenderBackend() override;

    HYP_FORCE_INLINE VulkanInstance* GetInstance() const
    {
        return m_instance;
    }

    const VulkanDeviceRef& GetDevice() const;

    virtual RendererResult Initialize(AppContextBase& appContext) override;
    virtual RendererResult Destroy() override;

    virtual const IRenderConfig& GetRenderConfig() const override
    {
        return *m_renderConfig;
    }

    virtual SwapchainBase* GetSwapchain() const override;

    virtual AsyncComputeBase* GetAsyncCompute() const override
    {
        return m_asyncCompute;
    }

    virtual FrameBase* GetCurrentFrame() const override;
    virtual FrameBase* PrepareNextFrame() override;
    virtual void PresentFrame(FrameBase* frame) override;

    virtual DescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) override;

    virtual DescriptorTableRef MakeDescriptorTable(const DescriptorTableDeclaration* decl) override;

    virtual GraphicsPipelineRef MakeGraphicsPipeline(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable,
        Span<const FramebufferRef> framebuffers,
        const RenderableAttributeSet& attributes) override;

    virtual ComputePipelineRef MakeComputePipeline(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable) override;

    virtual RaytracingPipelineRef MakeRaytracingPipeline(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable) override;

    virtual GpuBufferRef MakeGpuBuffer(GpuBufferType bufferType, SizeType size, SizeType alignment = 0) override;

    virtual ImageRef MakeImage(const TextureDesc& textureDesc) override;

    virtual ImageViewRef MakeImageView(const ImageRef& image) override;
    virtual ImageViewRef MakeImageView(const ImageRef& image, uint32 mipIndex, uint32 numMips, uint32 faceIndex, uint32 numFaces) override;

    virtual SamplerRef MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode) override;

    virtual FramebufferRef MakeFramebuffer(Vec2u extent, uint32 numViews = 1) override;
    virtual FramebufferRef MakeFramebuffer(Vec2u extent, RenderPassStage stage, uint32 numViews = 1) override;

    virtual FrameRef MakeFrame(uint32 frameIndex) override;

    virtual ShaderRef MakeShader(const RC<CompiledShader>& compiledShader) override;

    virtual BLASRef MakeBLAS(
        const GpuBufferRef& packedVerticesBuffer,
        const GpuBufferRef& packedIndicesBuffer,
        const Handle<Material>& material,
        const Matrix4& transform) override;
    virtual TLASRef MakeTLAS() override;

    virtual TextureFormat GetDefaultFormat(DefaultImageFormat type) const override;

    virtual bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const override;
    virtual TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const override;

    virtual QueryImageCapabilitiesResult QueryImageCapabilities(const TextureDesc& textureDesc) const override;

    virtual UniquePtr<SingleTimeCommands> GetSingleTimeCommands() override;

    virtual Delegate<void, SwapchainBase*>& GetOnSwapchainRecreatedDelegate() override
    {
        return OnSwapchainRecreated;
    }

    HYP_API RendererResult CreateDescriptorSet(const VulkanDescriptorSetLayoutWrapperRef& layout, VkDescriptorSet& outVkDescriptorSet);
    HYP_API RendererResult DestroyDescriptorSet(VkDescriptorSet vkDescriptorSet);
    HYP_API RendererResult GetOrCreateVkDescriptorSetLayout(const DescriptorSetLayout& layout, VulkanDescriptorSetLayoutWrapperRef& outRef);

private:
    Delegate<void, SwapchainBase*> OnSwapchainRecreated;

    VulkanInstance* m_instance;

    IRenderConfig* m_renderConfig;

    CrashHandler m_crashHandler;

    VulkanDescriptorSetManager* m_descriptorSetManager;

    AsyncComputeBase* m_asyncCompute;

    HashMap<DefaultImageFormat, TextureFormat> m_defaultFormats;

    bool m_shouldRecreateSwapchain;
};

} // namespace hyperion
