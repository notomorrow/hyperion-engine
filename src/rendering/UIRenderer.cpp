/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/UIRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

#include <core/logging/Logger.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/system/AppContext.hpp>

#include <ui/UIStage.hpp>
#include <ui/UIText.hpp>

#include <util/fs/FsUtil.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <util/MeshBuilder.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(RebuildProxyGroups_UI) : renderer::RenderCommand
{
    RC<EntityDrawCollection>            collection;
    Array<RenderProxy>                  added_proxies;
    Array<ID<Entity>>                   removed_proxies;
    FlatMap<ID<Entity>, RenderProxy>    changed_proxies;

    Array<ID<Entity>>                   proxy_ordering;

    FramebufferRef                      framebuffer;
    Optional<RenderableAttributeSet>    override_attributes;

    RENDER_COMMAND(RebuildProxyGroups_UI)(
        const RC<EntityDrawCollection> &collection,
        Array<RenderProxy> &&added_proxies,
        Array<ID<Entity>> &&removed_proxies,
        FlatMap<ID<Entity>, RenderProxy> &&changed_proxies,
        const Array<ID<Entity>> &proxy_ordering,
        const FramebufferRef &framebuffer = nullptr,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    ) : collection(collection),
        added_proxies(std::move(added_proxies)),
        removed_proxies(std::move(removed_proxies)),
        changed_proxies(std::move(changed_proxies)),
        proxy_ordering(proxy_ordering),
        framebuffer(framebuffer),
        override_attributes(override_attributes)
    {
    }

    virtual ~RENDER_COMMAND(RebuildProxyGroups_UI)() override
    {
        SafeRelease(std::move(framebuffer));
    }

    RenderableAttributeSet GetMergedRenderableAttributes(const RenderableAttributeSet &entity_attributes) const
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups: GetMergedRenderableAttributes");

        RenderableAttributeSet attributes = entity_attributes;

        // @FIXME: This is going to be quite slow, adding a reference for each item.
        if (framebuffer.IsValid()) {
            attributes.SetFramebuffer(framebuffer);
        }

        if (override_attributes.HasValue()) {
            if (const ShaderDefinition &override_shader_definition = override_attributes->GetShaderDefinition()) {
                attributes.SetShaderDefinition(override_shader_definition);
            }
            
            ShaderDefinition shader_definition = override_attributes->GetShaderDefinition().IsValid()
                ? override_attributes->GetShaderDefinition()
                : attributes.GetShaderDefinition();

#ifdef HYP_DEBUG_MODE
            AssertThrow(shader_definition.IsValid());
#endif

            // Check for varying vertex attributes on the override shader compared to the entity's vertex
            // attributes. If there is not a match, we should switch to a version of the override shader that
            // has matching vertex attribs.
            const VertexAttributeSet mesh_vertex_attributes = attributes.GetMeshAttributes().vertex_attributes;

            if (mesh_vertex_attributes != shader_definition.GetProperties().GetRequiredVertexAttributes()) {
                shader_definition.properties.SetRequiredVertexAttributes(mesh_vertex_attributes);
            }

            MaterialAttributes new_material_attributes = override_attributes->GetMaterialAttributes();
            new_material_attributes.shader_definition = shader_definition;
            // do not override bucket!
            new_material_attributes.bucket = attributes.GetMaterialAttributes().bucket;

            attributes.SetMaterialAttributes(new_material_attributes);
        }

        return attributes;
    }

    void BuildProxyGroupsInOrder()
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups: BuildProxyGroupsInOrder");

        collection->ClearProxyGroups();

        struct
        {
            HashCode    attributes_hash_code;
            uint32      drawable_layer = ~0u;
        } last_render_proxy_group;

        RenderProxyList &proxy_list = collection->GetProxyList(ThreadType::THREAD_TYPE_RENDER);

        for (const ID<Entity> entity : proxy_ordering)
        {
            RenderProxy *proxy = proxy_list.GetProxyForEntity(entity);

            if (!proxy) {
                continue;
            }

            const Handle<Mesh> &mesh = proxy->mesh;
            const Handle<Material> &material = proxy->material;

            if (!mesh.IsValid() || !material.IsValid()) {
                continue;
            }

            RenderableAttributeSet attributes = GetMergedRenderableAttributes(RenderableAttributeSet {
                mesh->GetMeshAttributes(),
                material->GetRenderAttributes()
            });

            const PassType pass_type = BucketToPassType(attributes.GetMaterialAttributes().bucket);

            // skip non-UI items
            if (pass_type != PASS_TYPE_UI) {
                continue;
            }

            if (last_render_proxy_group.drawable_layer != ~0u && last_render_proxy_group.attributes_hash_code == attributes.GetHashCode()) {
                // Set drawable layer on the attributes so it is grouped properly.
                attributes.SetDrawableLayer(last_render_proxy_group.drawable_layer);

                RenderProxyGroup &render_proxy_group = collection->GetProxyGroups()[pass_type][attributes];
                AssertThrow(render_proxy_group.GetRenderGroup().IsValid());

                render_proxy_group.AddRenderProxy(*proxy);
            } else {
                last_render_proxy_group = {
                    attributes.GetHashCode(),
                    last_render_proxy_group.drawable_layer + 1
                };

                attributes.SetDrawableLayer(last_render_proxy_group.drawable_layer);

                RenderProxyGroup &render_proxy_group = collection->GetProxyGroups()[pass_type][attributes];

                if (!render_proxy_group.GetRenderGroup().IsValid()) {
                    // Create RenderGroup
                    Handle<RenderGroup> render_group = CreateObject<RenderGroup>(
                        g_shader_manager->GetOrCreate(attributes.GetShaderDefinition()),
                        attributes,
                        RenderGroupFlags::NONE /* disable parallel rendering, to preserve number of command buffers */
                    );

                    InitObject(render_group);

                    HYP_LOG(UI, LogLevel::DEBUG, "Create render group {} (#{})", attributes.GetHashCode().Value(), render_group.GetID().Value());

#ifdef HYP_DEBUG_MODE
                    if (!render_group.IsValid()) {
                        HYP_LOG(UI, LogLevel::ERR, "Render group not valid for attribute set {}!", attributes.GetHashCode().Value());

                        continue;
                    }
#endif

                    InitObject(render_group);

                    render_proxy_group.SetRenderGroup(render_group);
                }

                render_proxy_group.AddRenderProxy(*proxy);
            }
        }

        collection->RemoveEmptyProxyGroups();
    }

    bool RemoveRenderProxy(RenderProxyList &proxy_list, ID<Entity> entity)
    {
        HYP_SCOPE;

        bool removed = false;

        for (auto &proxy_groups : collection->GetProxyGroups()) {
            for (auto &it : proxy_groups) {
                removed |= it.second.RemoveRenderProxy(entity);
            }
        }

        proxy_list.MarkToRemove(entity);

        return removed;
    }

    virtual Result operator()() override
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups");

        RenderProxyList &proxy_list = collection->GetProxyList(ThreadType::THREAD_TYPE_RENDER);

        for (const auto &it : changed_proxies) {
            // Remove it, then add it back (changed proxies will be included in the added proxies list)
            AssertThrow(RemoveRenderProxy(proxy_list, it.first));
        }

        for (RenderProxy &proxy : added_proxies) {
            proxy_list.Add(proxy.entity.GetID(), std::move(proxy));
        }

        for (const ID<Entity> &entity : removed_proxies) {
            proxy_list.MarkToRemove(entity);
        }

        proxy_list.Advance(RenderProxyListAdvanceAction::PERSIST);

        BuildProxyGroupsInOrder();

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateUIRendererFramebuffer) : renderer::RenderCommand
{
    RC<UIRenderer>  ui_renderer;

    RENDER_COMMAND(CreateUIRendererFramebuffer)(RC<UIRenderer> ui_renderer)
        : ui_renderer(ui_renderer)
    {
        AssertThrow(ui_renderer != nullptr);
    }

    virtual ~RENDER_COMMAND(CreateUIRendererFramebuffer)() override = default;

    virtual Result operator()() override
    {
        ui_renderer->CreateFramebuffer();

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region UIRenderList

UIRenderList::UIRenderList()
    : RenderList()
{
}

UIRenderList::UIRenderList(const Handle<Camera> &camera)
    : RenderList(camera)
{
}

UIRenderList::~UIRenderList() = default;

void UIRenderList::ResetOrdering()
{
    m_proxy_ordering.Clear();
}

void UIRenderList::PushEntityToRender(ID<Entity> entity, const RenderProxy &proxy)
{
    RenderList::PushEntityToRender(entity, proxy);

    m_proxy_ordering.PushBack(entity);
}

RenderListCollectionResult UIRenderList::PushUpdatesToRenderThread(const FramebufferRef &framebuffer, const Optional<RenderableAttributeSet> &override_attributes)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME);
    AssertThrow(m_draw_collection != nullptr);

    RenderProxyList &proxy_list = m_draw_collection->GetProxyList(ThreadType::THREAD_TYPE_GAME);

    RenderListCollectionResult collection_result { };
    collection_result.num_added_entities = proxy_list.GetAddedEntities().Count();
    collection_result.num_removed_entities = proxy_list.GetRemovedEntities().Count();
    collection_result.num_changed_entities = proxy_list.GetChangedEntities().Count();

    if (collection_result.NeedsUpdate()) {
        Array<ID<Entity>> removed_proxies;
        proxy_list.GetRemovedEntities(removed_proxies);

        Array<RenderProxy *> added_proxies_ptrs;
        proxy_list.GetAddedEntities(added_proxies_ptrs, true /* include_changed */);

        FlatMap<ID<Entity>, RenderProxy> changed_proxies = std::move(proxy_list.GetChangedRenderProxies());

        if (added_proxies_ptrs.Any() || removed_proxies.Any() || changed_proxies.Any()) {
            Array<RenderProxy> added_proxies;
            added_proxies.Resize(added_proxies_ptrs.Size());

            for (uint i = 0; i < added_proxies_ptrs.Size(); i++) {
                AssertThrow(added_proxies_ptrs[i] != nullptr);

                // Copy the proxy to be added on the render thread
                added_proxies[i] = *added_proxies_ptrs[i];
            }

            PUSH_RENDER_COMMAND(
                RebuildProxyGroups_UI,
                m_draw_collection,
                std::move(added_proxies),
                std::move(removed_proxies),
                std::move(changed_proxies),
                m_proxy_ordering,
                framebuffer,
                override_attributes
            );
        }
    }

    proxy_list.Advance(RenderProxyListAdvanceAction::CLEAR);

    return collection_result;
}

void UIRenderList::CollectDrawCalls(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    using IteratorType = FlatMap<RenderableAttributeSet, RenderProxyGroup>::Iterator;

    Array<IteratorType> iterators;

    for (auto &proxy_groups : m_draw_collection->GetProxyGroups()) {
        for (auto &it : proxy_groups) {
            const RenderableAttributeSet &attributes = it.first;

            if (attributes.GetMaterialAttributes().bucket != BUCKET_UI) {
                continue;
            }

            iterators.PushBack(&it);
        }
    }

    TaskSystem::GetInstance().ParallelForEach(
        TaskThreadPoolName::THREAD_POOL_RENDER,
        iterators,
        [this](IteratorType it, uint, uint)
        {
            RenderProxyGroup &proxy_group = it->second;

            const Handle<RenderGroup> &render_group = proxy_group.GetRenderGroup();
            AssertThrow(render_group.IsValid());

            render_group->CollectDrawCalls(proxy_group.GetRenderProxies());
        }
    );
}

void UIRenderList::ExecuteDrawCalls(Frame *frame) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    
    AssertThrow(m_draw_collection != nullptr);

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    AssertThrowMsg(m_camera.IsValid(), "Cannot render with invalid Camera");

    const FramebufferRef &framebuffer = m_camera->GetFramebuffer();
    AssertThrow(framebuffer.IsValid());

    framebuffer->BeginCapture(command_buffer, frame_index);

    g_engine->GetRenderState().BindCamera(m_camera.Get());

    using IteratorType = FlatMap<RenderableAttributeSet, RenderProxyGroup>::ConstIterator;
    Array<IteratorType> iterators;

    for (const auto &proxy_groups : m_draw_collection->GetProxyGroups()) {
        for (const auto &it : proxy_groups) {
            iterators.PushBack(&it);
        }
    }

    {
        HYP_NAMED_SCOPE("Sort proxy groups by layer");

        std::sort(iterators.Begin(), iterators.End(), [](IteratorType lhs, IteratorType rhs) -> bool
        {
            return lhs->first.GetDrawableLayer() < rhs->first.GetDrawableLayer();
        });
    }

    // HYP_LOG(UI, LogLevel::DEBUG, " UI rendering {} render groups", iterators.Size());

    for (SizeType index = 0; index < iterators.Size(); index++) {
        const auto &it = *iterators[index];

        const RenderableAttributeSet &attributes = it.first;
        const RenderProxyGroup &proxy_group = it.second;

        if (attributes.GetMaterialAttributes().bucket != BUCKET_UI) {
            continue;
        }

        AssertThrow(proxy_group.GetRenderGroup().IsValid());

        if (framebuffer.IsValid()) {
            AssertThrowMsg(
                attributes.GetFramebuffer() == framebuffer,
                "Given Framebuffer does not match RenderList item's framebuffer -- invalid data passed?"
            );
        }

        proxy_group.GetRenderGroup()->PerformRendering(frame);
    }

    g_engine->GetRenderState().UnbindCamera();

    framebuffer->EndCapture(command_buffer, frame_index);
}

#pragma endregion UIRenderList

#pragma region UITextRenderer

struct alignas(16) UITextCharacterShaderData
{
    Matrix4 transform;
    Vec2f   texcoord_start;
    Vec2f   texcoord_end;
};

struct alignas(16) UITextShaderData
{
    Matrix4 transform;
};

class UITextRenderer
{
public:
    UITextRenderer(const FramebufferRef &framebuffer)
        : m_framebuffer(framebuffer),
          m_character_instance_buffer_offset(0),
          m_text_instance_buffer_offset(0)
    {
        static constexpr SizeType initial_character_instance_buffer_size = sizeof(UITextCharacterShaderData) * 1024;
        static constexpr SizeType initial_text_instance_buffer_size = sizeof(UITextShaderData) * 16;

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_character_instance_buffers[frame_index] = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
            DeferCreate(m_character_instance_buffers[frame_index], g_engine->GetGPUDevice(), initial_character_instance_buffer_size);

            m_text_instance_buffers[frame_index] = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
            DeferCreate(m_text_instance_buffers[frame_index], g_engine->GetGPUDevice(), initial_text_instance_buffer_size);
        }

        m_shader = g_shader_manager->GetOrCreate(NAME("UIText"), ShaderProperties(static_mesh_vertex_attributes));
        AssertThrow(m_shader.IsValid());

        // m_quad_mesh = UIObjectQuadMeshHelper::GetQuadMesh();

        m_quad_mesh = MeshBuilder::Quad();
        InitObject(m_quad_mesh);
    }

    void BeforeRender()
    {
        m_character_instance_buffer_offset = 0;
        m_text_instance_buffer_offset = 0;

        // reset offset
        for (auto &it : m_text_render_groups) {
            it.second.second = 0;
        }
    }

    void RenderText(Frame *frame, const UITextRenderData &render_data, const Matrix4 &transform)
    {
        HYP_SCOPE;
        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        const uint frame_index = frame->GetFrameIndex();

        const GPUBufferRef &character_instance_buffer = m_character_instance_buffers[frame_index];
        const GPUBufferRef &text_instance_buffer = m_text_instance_buffers[frame_index];
        
        const SizeType characters_byte_size = render_data.characters.Size() * sizeof(UITextCharacterShaderData);
        const SizeType total_characters_byte_size = m_character_instance_buffer_offset + characters_byte_size;
        
        const SizeType total_text_byte_size = m_text_instance_buffer_offset + sizeof(UITextShaderData);

        bool was_character_instance_buffer_rebuilt = false;

        if (total_characters_byte_size > character_instance_buffer->Size()) {
            ByteBuffer previous_data;
            previous_data.SetSize(character_instance_buffer->Size());

            character_instance_buffer->Read(
                g_engine->GetGPUDevice(),
                character_instance_buffer->Size(),
                previous_data.Data()
            );

            HYPERION_ASSERT_RESULT(character_instance_buffer->EnsureCapacity(
                g_engine->GetGPUDevice(),
                total_characters_byte_size,
                &was_character_instance_buffer_rebuilt
            ));

            character_instance_buffer->Copy(
                g_engine->GetGPUDevice(),
                previous_data.Size(),
                previous_data.Data()
            );
        }

        bool was_text_instance_buffer_rebuilt = false;

        if (total_text_byte_size > text_instance_buffer->Size()) {
            ByteBuffer previous_data;
            previous_data.SetSize(text_instance_buffer->Size());

            text_instance_buffer->Read(
                g_engine->GetGPUDevice(),
                text_instance_buffer->Size(),
                previous_data.Data()
            );

            HYPERION_ASSERT_RESULT(text_instance_buffer->EnsureCapacity(
                g_engine->GetGPUDevice(),
                total_text_byte_size,
                &was_text_instance_buffer_rebuilt
            ));

            text_instance_buffer->Copy(
                g_engine->GetGPUDevice(),
                previous_data.Size(),
                previous_data.Data()
            );
        }

        AssertThrow(text_instance_buffer->Size() >= total_text_byte_size);
        AssertThrow(character_instance_buffer->Size() >= total_characters_byte_size);

        { // copy characters shader data
            Array<UITextCharacterShaderData> characters;
            characters.Resize(render_data.characters.Size());

            for (SizeType index = 0; index < render_data.characters.Size(); index++) {
                characters[index] = UITextCharacterShaderData {
                    render_data.characters[index].transform,
                    render_data.characters[index].texcoord_start,
                    render_data.characters[index].texcoord_end
                };
            }

            character_instance_buffer->Copy(
                g_engine->GetGPUDevice(),
                m_character_instance_buffer_offset,
                characters_byte_size,
                characters.Data()
            );
        }

        { // copy text shader data
            UITextShaderData text_shader_data;
            text_shader_data.transform = transform;

            text_instance_buffer->Copy(
                g_engine->GetGPUDevice(),
                m_text_instance_buffer_offset,
                sizeof(UITextShaderData),
                &text_shader_data
            );
        }

        const Handle<RenderGroup> &render_group = GetOrCreateRenderGroup(render_data.font_atlas_texture);

        const uint descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("UITextDescriptorSet"));
        AssertThrow(descriptor_set_index != ~0u);

        if (was_character_instance_buffer_rebuilt || was_text_instance_buffer_rebuilt) {
            for (auto &it : m_text_render_groups) {
                for (const Handle<RenderGroup> &render_group : it.second.first) {
                    const DescriptorSetRef &descriptor_set = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(descriptor_set_index, frame_index);
                    descriptor_set->SetElement(NAME("CharacterInstanceBuffer"), character_instance_buffer);
                    descriptor_set->SetElement(NAME("TextInstanceBuffer"), text_instance_buffer);
                    descriptor_set->Update(g_engine->GetGPUDevice());
                }
            }
        }

        render_group->GetPipeline()->Bind(frame->GetCommandBuffer());

        DebugLog(LogType::Debug, "Render from text instance buffer at offset: %u\tbuffer size: %llu\n", m_text_instance_buffer_offset, text_instance_buffer->Size());

        if (renderer::RenderConfig::ShouldCollectUniqueDrawCallPerMaterial()) {
            render_group->GetPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
                frame->GetCommandBuffer(),
                frame_index,
                render_group->GetPipeline(),
                {
                    {
                        NAME("UITextDescriptorSet"),
                        {
                            { NAME("TextInstanceBuffer"), uint32(m_text_instance_buffer_offset) }
                        }
                    },
                    {
                        NAME("Scene"),
                        {
                            { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                            { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                            { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                            { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                            { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                        }
                    },
                    {
                        NAME("Object"),
                        {
                            { NAME("MaterialsBuffer"), 0 },
                            { NAME("SkeletonsBuffer"), 0 },
                            { NAME("EntityInstanceBatchesBuffer"), 0 }
                        }
                    }
                }
            );
        } else {
            render_group->GetPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
                frame->GetCommandBuffer(),
                frame_index,
                render_group->GetPipeline(),
                {
                    {
                        NAME("UITextDescriptorSet"),
                        {
                            { NAME("TextInstanceBuffer"), uint32(m_text_instance_buffer_offset) }
                        }
                    },
                    {
                        NAME("Scene"),
                        {
                            { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                            { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                            { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                            { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                            { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                        }
                    },
                    {
                        NAME("Object"),
                        {
                            { NAME("SkeletonsBuffer"), 0 },
                            { NAME("EntityInstanceBatchesBuffer"), 0 }
                        }
                    }
                }
            );
        }

        m_quad_mesh->Render(frame->GetCommandBuffer(), render_data.characters.Size(), m_character_instance_buffer_offset / sizeof(UITextCharacterShaderData));

        m_character_instance_buffer_offset += characters_byte_size;
        m_text_instance_buffer_offset += sizeof(UITextShaderData);
    }

private:
    DescriptorTableRef CreateDescriptorTable(const Handle<Texture> &font_atlas_texture)
    {
        renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);
        AssertThrow(descriptor_table != nullptr);

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("UITextDescriptorSet"), frame_index);
            AssertThrow(descriptor_set != nullptr);

            descriptor_set->SetElement(NAME("CharacterInstanceBuffer"), m_character_instance_buffers[frame_index]);
            descriptor_set->SetElement(NAME("TextInstanceBuffer"), m_text_instance_buffers[frame_index]);
            descriptor_set->SetElement(NAME("FontAtlasTexture"), font_atlas_texture->GetImageView());
        }

        DeferCreate(descriptor_table, g_engine->GetGPUDevice());

        return descriptor_table;
    }

    const Handle<RenderGroup> &GetOrCreateRenderGroup(const Handle<Texture> &font_atlas_texture)
    {
        auto it = m_text_render_groups.Find(font_atlas_texture.GetID());

        if (it == m_text_render_groups.End()) {
            it = m_text_render_groups.Insert({ }).first;
        }

        auto &pair = it->second;

        Array<Handle<RenderGroup>> &render_groups = pair.first;
        uint offset = pair.second++;

        if (offset < render_groups.Size()) {
            return render_groups[offset];
        }
        
        Handle<RenderGroup> &render_group = render_groups.PushBack(CreateObject<RenderGroup>(
            m_shader,
            RenderableAttributeSet(
                MeshAttributes {
                    .vertex_attributes = static_mesh_vertex_attributes
                },
                MaterialAttributes {
                    .bucket         = Bucket::BUCKET_TRANSLUCENT,
                    .fill_mode      = FillMode::FILL,
                    .blend_function = BlendFunction::None(),
                    .cull_faces     = FaceCullMode::NONE
                }
            ),
            CreateDescriptorTable(font_atlas_texture),
            RenderGroupFlags::NONE
        ));

        render_group->AddFramebuffer(m_framebuffer);

        InitObject(render_group);

        return render_group;
    }

    FramebufferRef                                                  m_framebuffer;
    ShaderRef                                                       m_shader;
    Handle<Mesh>                                                    m_quad_mesh;

    FixedArray<GPUBufferRef, max_frames_in_flight>                  m_character_instance_buffers;
    SizeType                                                        m_character_instance_buffer_offset;

    FixedArray<GPUBufferRef, max_frames_in_flight>                  m_text_instance_buffers;
    SizeType                                                        m_text_instance_buffer_offset;

    HashMap<ID<Texture>, Pair<Array<Handle<RenderGroup>>, uint>>    m_text_render_groups;
};

#pragma endregion UITextRenderer

#pragma region UIRenderer

UIRenderer::UIRenderer(Name name, RC<UIStage> ui_stage)
    : RenderComponent(name),
      m_ui_stage(std::move(ui_stage))
{
}

UIRenderer::~UIRenderer()
{
    SafeRelease((std::move(m_framebuffer)));
    g_engine->GetFinalPass()->SetUITexture(Handle<Texture>::empty);
}

void UIRenderer::Init()
{
    HYP_SCOPE;

    m_on_gbuffer_resolution_changed_handle = g_engine->GetDelegates().OnAfterSwapchainRecreated.Bind([this]()
    {
        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        SafeRelease(std::move(m_framebuffer));
        g_engine->GetFinalPass()->SetUITexture(Handle<Texture>::empty);

        CreateFramebuffer();
    });

    PUSH_RENDER_COMMAND(CreateUIRendererFramebuffer, RefCountedPtrFromThis());

    AssertThrow(m_ui_stage != nullptr);

    AssertThrow(m_ui_stage->GetScene() != nullptr);
    AssertThrow(m_ui_stage->GetScene()->GetCamera().IsValid());

    m_render_list.SetCamera(m_ui_stage->GetScene()->GetCamera());
}

void UIRenderer::CreateFramebuffer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const Vec2i surface_size = Vec2i(g_engine->GetAppContext()->GetMainWindow()->GetDimensions());
    
    //Vec2i(g_engine->GetDeferredRenderer()->GetGBuffer()->GetResolution());

    m_ui_stage->GetScene()->GetEntityManager()->PushCommand([ui_stage = m_ui_stage, surface_size](EntityManager &mgr, GameCounter::TickUnit delta)
    {
        ui_stage->SetSurfaceSize(surface_size);
    });

    m_framebuffer = g_engine->GetDeferredRenderer()->GetGBuffer()->GetBucket(Bucket::BUCKET_UI).GetFramebuffer();
    AssertThrow(m_framebuffer.IsValid());

    m_ui_stage->GetScene()->GetCamera()->SetFramebuffer(m_framebuffer);

    g_engine->GetFinalPass()->SetUITexture(CreateObject<Texture>(
        m_framebuffer->GetAttachment(0)->GetImage(),
        m_framebuffer->GetAttachment(0)->GetImageView()
    ));

    m_text_renderer.Reset(new UITextRenderer(m_framebuffer));
}

// called from game thread
void UIRenderer::InitGame() { }

void UIRenderer::OnRemoved()
{
    g_engine->GetFinalPass()->SetUITexture(Handle<Texture>::empty);
}

void UIRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    struct RENDER_COMMAND(Temp_ClearTextsToRender) : renderer::RenderCommand
    {
        UIRenderer  *ui_renderer;

        RENDER_COMMAND(Temp_ClearTextsToRender)(UIRenderer *ui_renderer)
            : ui_renderer(ui_renderer)
        {
        }

        virtual ~RENDER_COMMAND(Temp_ClearTextsToRender)() = default;

        virtual renderer::Result operator()() override
        {
            ui_renderer->m_text_render_data.Clear();

            HYPERION_RETURN_OK;
        }
    };

    struct RENDER_COMMAND(Temp_PushTextToRender) : renderer::RenderCommand
    {
        UIRenderer              *ui_renderer;
        RC<UITextRenderData>    text_render_data;
        Matrix4                 transform;

        RENDER_COMMAND(Temp_PushTextToRender)(UIRenderer *ui_renderer, const RC<UITextRenderData> &text_render_data, const Matrix4 &transform)
            : ui_renderer(ui_renderer),
              text_render_data(text_render_data),
              transform(transform)
        {
        }

        virtual ~RENDER_COMMAND(Temp_PushTextToRender)() = default;

        virtual renderer::Result operator()() override
        {
            ui_renderer->m_text_render_data.EmplaceBack(std::move(text_render_data), transform);

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(Temp_ClearTextsToRender, this);

    m_render_list.ResetOrdering();

    m_ui_stage->CollectObjects([this](UIObject *object)
    {
        AssertThrow(object != nullptr);

        const NodeProxy &node = object->GetNode();
        AssertThrow(node.IsValid());

        if (object->GetType() == UIObjectType::TEXT) {
            PUSH_RENDER_COMMAND(Temp_PushTextToRender, this, static_cast<UIText *>(object)->GetRenderData(), node->GetWorldTransform().GetMatrix());
        } else {
            const ID<Entity> entity = node->GetEntity();

            MeshComponent *mesh_component = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(entity);
            AssertThrow(mesh_component != nullptr);
            AssertThrow(mesh_component->proxy != nullptr);

            m_render_list.PushEntityToRender(entity, *mesh_component->proxy);
        }
    });

    m_render_list.PushUpdatesToRenderThread(m_ui_stage->GetScene()->GetCamera()->GetFramebuffer());
}

void UIRenderer::OnRender(Frame *frame)
{
    HYP_SCOPE;

    m_text_renderer->BeforeRender();

    g_engine->GetRenderState().BindScene(m_ui_stage->GetScene());
    g_engine->GetRenderState().BindCamera(m_render_list.GetCamera().Get());

    // m_render_list.CollectDrawCalls(frame);
    // m_render_list.ExecuteDrawCalls(frame);


    m_render_list.GetCamera()->GetFramebuffer()->BeginCapture(frame->GetCommandBuffer(), frame->GetFrameIndex());

    for (auto &it : m_text_render_data) {
        if (!it.first) {
            continue;
        }
        m_text_renderer->RenderText(frame, *it.first, it.second);
    }

    // // testing
    // if (m_ui_stage->GetDefaultFontAtlas()) {
    //     const auto &font_atlas_texture = m_ui_stage->GetDefaultFontAtlas()->GetAtlases()->GetAtlasForPixelSize(12);

    //     if (font_atlas_texture) {
    //         Array<UITextCharacterShaderData> characters;
    //         characters.PushBack(UITextCharacterShaderData {
    //             Vec2i { 0, 0 },
    //             Vec2f { 0.0f, 0.0f },
    //             Vec2f { 1.0f, 1.0f }
    //         });
    //         characters.PushBack(UITextCharacterShaderData {
    //             Vec2i { 25, 25 },
    //             Vec2f { 0.2f, 0.2f },
    //             Vec2f { 1.0f, 1.0f }
    //         });
    //     }

    // }

    m_render_list.GetCamera()->GetFramebuffer()->EndCapture(frame->GetCommandBuffer(), frame->GetFrameIndex());

    g_engine->GetRenderState().UnbindCamera();
    g_engine->GetRenderState().UnbindScene();
}

#pragma endregion UIRenderer

} // namespace hyperion
