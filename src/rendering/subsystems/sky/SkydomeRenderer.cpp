/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/subsystems/sky/SkydomeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderTexture.hpp>

#include <scene/World.hpp>
#include <scene/Texture.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/CameraComponent.hpp>

#include <core/threading/Scheduler.hpp>

#include <asset/Assets.hpp>

#include <Engine.hpp>

namespace hyperion {

SkydomeRenderer::SkydomeRenderer(Name name, Vec2u dimensions)
    : RenderSubsystem(name, 60),
      m_dimensions(dimensions)
{
    m_cubemap = CreateObject<Texture>(
        TextureDesc {
            ImageType::TEXTURE_TYPE_CUBEMAP,
            InternalFormat::RGBA8,
            Vec3u { m_dimensions.x, m_dimensions.y, 1 },
            FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
            FilterMode::TEXTURE_FILTER_LINEAR
        }
    );

    InitObject(m_cubemap);

    m_camera = CreateObject<Camera>(
        90.0f,
        -int(m_dimensions.x), int(m_dimensions.y),
        0.1f, 10000.0f
    );

    m_camera->SetName(Name::Unique("SkydomeRendererCamera"));
    m_camera->SetViewMatrix(Matrix4::LookAt(Vec3f::UnitZ(), Vec3f::Zero(), Vec3f::UnitY()));
    
    InitObject(m_camera);

    m_virtual_scene = CreateObject<Scene>(nullptr, m_camera);
    m_virtual_scene->SetName(Name::Unique("SkydomeRendererScene"));

    NodeProxy camera_node = m_virtual_scene->GetRoot()->AddChild();
    camera_node->SetName("SkydomeRendererCamera");
    
    Handle<Entity> camera_entity = m_virtual_scene->GetEntityManager()->AddEntity();
    m_virtual_scene->GetEntityManager()->AddComponent<CameraComponent>(camera_entity, CameraComponent {
        m_camera
    });

    camera_node->SetEntity(camera_entity);

    m_env_probe = CreateObject<EnvProbe>(
        m_virtual_scene,
        BoundingBox(Vec3f(-1000.0f), Vec3f(1000.0f)),
        m_dimensions,
        ENV_PROBE_TYPE_SKY
    );
}

void SkydomeRenderer::Init()
{
}

void SkydomeRenderer::InitGame()
{
    g_engine->GetWorld()->AddScene(m_virtual_scene);
    InitObject(m_virtual_scene);

    InitObject(m_env_probe);
    m_env_probe->EnqueueBind();

    auto dome_node_asset = g_asset_manager->Load<Node>("models/inv_sphere.obj");

    if (dome_node_asset.HasValue()) {
        NodeProxy dome_node = NodeProxy(dome_node_asset->Result());
        dome_node.Scale(Vec3f(10.0f));
        dome_node.LockTransform();

        m_virtual_scene->GetRoot()->AddChild(dome_node);
    }
}

void SkydomeRenderer::OnRemoved()
{
    if (m_env_probe) {
        m_env_probe->EnqueueUnbind();
        m_env_probe.Reset();
    }

    m_camera.Reset();
    m_cubemap.Reset();

    Threads::GetThread(g_game_thread)->GetScheduler().Enqueue(
        HYP_STATIC_MESSAGE("Remove skydome scene from world"),
        [scene = m_virtual_scene]()
        {
            g_engine->GetWorld()->RemoveScene(scene);
        },
        TaskEnqueueFlags::FIRE_AND_FORGET
    );
}

void SkydomeRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    // Do nothing
}

void SkydomeRenderer::OnRender(Frame *frame)
{
    AssertThrow(m_env_probe.IsValid());

    if (!m_env_probe->IsReady()) {
        return;
    }

    // if (!m_env_probe->NeedsRender()) {
    //     return;
    // }

    m_env_probe->GetRenderResource().Render(frame);

    // Copy cubemap from env probe to cubemap texture
    const ImageRef &src_image = m_env_probe->GetRenderResource().GetTexture()->GetRenderResource().GetImage();
    const ImageRef &dst_image = m_cubemap->GetRenderResource().GetImage();

    src_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_SRC);
    dst_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_DST);

    dst_image->Blit(frame->GetCommandBuffer(), src_image);
    
    dst_image->GenerateMipmaps(g_engine->GetGPUDevice(), frame->GetCommandBuffer());

    src_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    dst_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
}

} // namespace hyperion