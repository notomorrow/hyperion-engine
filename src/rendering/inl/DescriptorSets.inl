#ifndef HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE
    #error "HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE is not defined"
#endif

#define HYP_DESCRIPTOR_SET(index, name) \
    static DescriptorTableDeclaration::DeclareSet desc_set_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, index, HYP_NAME_UNSAFE(name))

#define HYP_DESCRIPTOR_SRV(set_name, name, count) \
    static DescriptorTableDeclaration::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_SRV, HYP_STR(name), count)
#define HYP_DESCRIPTOR_UAV(set_name, name, count) \
    static DescriptorTableDeclaration::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_UAV, HYP_STR(name), count)
#define HYP_DESCRIPTOR_CBUFF(set_name, name, count, size) \
    static DescriptorTableDeclaration::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_CBUFF, HYP_STR(name), count, size)
#define HYP_DESCRIPTOR_SSBO(set_name, name, count, size) \
    static DescriptorTableDeclaration::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_SSBO, HYP_STR(name), count, size)
#define HYP_DESCRIPTOR_ACCELERATION_STRUCTURE(set_name, name, count) \
    static DescriptorTableDeclaration::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE, HYP_STR(name), count)
#define HYP_DESCRIPTOR_SAMPLER(set_name, name, count) \
    static DescriptorTableDeclaration::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_SAMPLER, HYP_STR(name), count)

HYP_DESCRIPTOR_SET(0, Global);
HYP_DESCRIPTOR_SRV(Global, GBufferTextures, num_gbuffer_textures);
HYP_DESCRIPTOR_SRV(Global, GBufferDepthTexture, 1);
HYP_DESCRIPTOR_SRV(Global, GBufferMipChain, 1);
HYP_DESCRIPTOR_SRV(Global, DeferredResult, 1);
HYP_DESCRIPTOR_SRV(Global, PostFXPreStack, 4);
HYP_DESCRIPTOR_SRV(Global, PostFXPostStack, 4);
HYP_DESCRIPTOR_SRV(Global, SSRUVTexture, 1);
HYP_DESCRIPTOR_SRV(Global, SSRSampleTexture, 1);
HYP_DESCRIPTOR_SRV(Global, SSRFinalTexture, 1);
HYP_DESCRIPTOR_SRV(Global, EnvProbeTextures, max_bound_reflection_probes);
HYP_DESCRIPTOR_SRV(Global, PointShadowMaps, max_bound_point_shadow_maps);
HYP_DESCRIPTOR_SRV(Global, DepthPyramidResult, 1);
HYP_DESCRIPTOR_SRV(Global, SSRResultImage, 1);
HYP_DESCRIPTOR_UAV(Global, SSRUVImage, 1);
HYP_DESCRIPTOR_UAV(Global, SSRSampleImage, 1);

HYP_DESCRIPTOR_SET(1, Scene);
HYP_DESCRIPTOR_SET(2, Object);
HYP_DESCRIPTOR_SET(3, Material);

#undef HYP_DESCRIPTOR_SET
#undef HYP_DESCRIPTOR_SRV
#undef HYP_DESCRIPTOR_UAV
#undef HYP_DESCRIPTOR_CBUFF
#undef HYP_DESCRIPTOR_SSBO
#undef HYP_DESCRIPTOR_ACCELERATION_STRUCTURE
#undef HYP_DESCRIPTOR_SAMPLER
