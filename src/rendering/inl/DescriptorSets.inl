
HYP_DESCRIPTOR_SSBO(Global, BlueNoiseBuffer, 1, sizeof(BlueNoiseBuffer), false);
HYP_DESCRIPTOR_CBUFF(Global, SphereSamplesBuffer, 1, sizeof(Vec4f) * 4096, false);
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear, 1);
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest, 1);
HYP_DESCRIPTOR_SRV(Global, UITexture, 1);
HYP_DESCRIPTOR_SRV(Global, FinalOutputTexture, 1);
HYP_DESCRIPTOR_SSBO(Global, ObjectsBuffer, 1, ~0u, false); // For instanced objects
HYP_DESCRIPTOR_SRV(Global, EnvProbeTextures, max_bound_reflection_probes);
HYP_DESCRIPTOR_SRV(Global, VoxelGridTexture, 1);
HYP_DESCRIPTOR_SRV(Global, LightFieldColorTexture, 1);
HYP_DESCRIPTOR_SRV(Global, LightFieldDepthTexture, 1);

HYP_DESCRIPTOR_SRV(Object, LightmapVolumeIrradianceTexture, 1);
HYP_DESCRIPTOR_SRV(Object, LightmapVolumeRadianceTexture, 1);
HYP_DESCRIPTOR_SSBO(Object, CurrentObject, 1, sizeof(EntityShaderData), true); // For non-instanced objects

HYP_DESCRIPTOR_SRV_COND(Scene, GBufferTextures, num_gbuffer_textures, g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(Scene, GBufferAlbedoTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(Scene, GBufferNormalsTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(Scene, GBufferMaterialTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(Scene, GBufferLightmapTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(Scene, GBufferVelocityTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(Scene, GBufferMaskTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(Scene, GBufferWSNormalsTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(Scene, GBufferTranslucentTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV(Scene, GBufferDepthTexture, 1);
HYP_DESCRIPTOR_SRV(Scene, GBufferMipChain, 1);
HYP_DESCRIPTOR_SRV(Scene, DeferredResult, 1);
HYP_DESCRIPTOR_SRV_COND(Scene, PostFXPreStack, 4, g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(Scene, PostFXPostStack, 4, g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV(Scene, SSRResultTexture, 1);
HYP_DESCRIPTOR_SRV(Scene, SSGIResultTexture, 1);
HYP_DESCRIPTOR_SRV(Scene, SSAOResultTexture, 1);
HYP_DESCRIPTOR_SRV(Scene, TAAResultTexture, 1);
HYP_DESCRIPTOR_SRV(Global, RTRadianceResultTexture, 1);
HYP_DESCRIPTOR_SRV(Scene, EnvGridIrradianceResultTexture, 1);
HYP_DESCRIPTOR_SRV(Scene, EnvGridRadianceResultTexture, 1);
HYP_DESCRIPTOR_SRV(Scene, ReflectionProbeResultTexture, 1);
HYP_DESCRIPTOR_SRV(Scene, DeferredIndirectResultTexture, 1);
HYP_DESCRIPTOR_SRV(Scene, DepthPyramidResult, 1);

HYP_DESCRIPTOR_SRV_COND(Material, Textures, max_bindless_resources, g_rendering_api->GetRenderConfig().IsBindlessSupported());
HYP_DESCRIPTOR_SRV_COND(Material, Textures, max_bound_textures, !g_rendering_api->GetRenderConfig().IsBindlessSupported());
