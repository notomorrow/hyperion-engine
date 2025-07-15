/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/EnvProbe.hpp>
#include <scene/View.hpp>
#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <rendering/Texture.hpp>

#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderDescriptorSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

static const TextureFormat reflectionProbeFormat = TF_RGBA16F;
static const TextureFormat shadowProbeFormat = TF_RG32F;

static FixedArray<Matrix4, 6> CreateCubemapMatrices(const BoundingBox& aabb, const Vec3f& origin)
{
    FixedArray<Matrix4, 6> viewMatrices;

    for (uint32 i = 0; i < 6; i++)
    {
        viewMatrices[i] = Matrix4::LookAt(
            origin,
            origin + Texture::cubemapDirections[i].first,
            Texture::cubemapDirections[i].second);
    }

    return viewMatrices;
}

#pragma region Render commands

struct RENDER_COMMAND(RenderPointLightShadow)
    : RenderCommand
{
    RenderWorld* world;
    RenderEnvProbe* envProbe;

    RENDER_COMMAND(RenderPointLightShadow)(RenderWorld* world, RenderEnvProbe* envProbe)
        : world(world),
          envProbe(envProbe)
    {
        // world->IncRef();
        // envProbe->IncRef();
    }

    virtual ~RENDER_COMMAND(RenderPointLightShadow)() override
    {
        world->DecRef();
        envProbe->DecRef();
    }

    virtual RendererResult operator()() override
    {
        FrameBase* frame = g_renderBackend->GetCurrentFrame();
        RenderSetup renderSetup { world, nullptr };

        envProbe->Render(frame, renderSetup);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

EnvProbe::EnvProbe()
    : EnvProbe(EPT_INVALID)
{
}

EnvProbe::EnvProbe(EnvProbeType envProbeType)
    : EnvProbe(envProbeType, BoundingBox(Vec3f(-25.0f), Vec3f(25.0f)), Vec2u { 256, 256 })
{
}

EnvProbe::EnvProbe(EnvProbeType envProbeType, const BoundingBox& aabb, const Vec2u& dimensions)
    : m_aabb(aabb),
      m_dimensions(dimensions),
      m_envProbeType(envProbeType),
      m_cameraNear(0.05f),
      m_cameraFar(aabb.GetRadius()),
      m_needsRenderCounter(0),
      m_renderResource(nullptr)
{
    m_entityInitInfo.receivesUpdate = true;
}

bool EnvProbe::IsVisible(ObjId<Camera> cameraId) const
{
    return m_visibilityBits.Test(cameraId.ToIndex());
}

void EnvProbe::SetIsVisible(ObjId<Camera> cameraId, bool isVisible)
{
    const bool previousValue = m_visibilityBits.Test(cameraId.ToIndex());

    m_visibilityBits.Set(cameraId.ToIndex(), isVisible);

    if (isVisible != previousValue)
    {
        Invalidate();
    }
}

EnvProbe::~EnvProbe()
{
    // temp!
    if (IsInitCalled())
    {
        if (m_view.IsValid())
        {
            m_view->GetRenderResource().DecRef();
        }
    }

    if (m_renderResource != nullptr)
    {
        // TEMP!
        m_renderResource->DecRef();

        FreeResource(m_renderResource);

        m_renderResource = nullptr;
    }
}

void EnvProbe::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
        {
            m_camera.Reset();

            if (m_renderResource != nullptr)
            {
                FreeResource(m_renderResource);

                m_renderResource = nullptr;
            }
        }));

    m_renderResource = AllocateResource<RenderEnvProbe>(this);

    if (!IsControlledByEnvGrid())
    {
        m_camera = CreateObject<Camera>(
            90.0f,
            -int(m_dimensions.x), int(m_dimensions.y),
            m_cameraNear, m_cameraFar);

        m_camera->SetName(NAME("EnvProbeCamera"));
        m_camera->SetViewMatrix(Matrix4::LookAt(Vec3f(0.0f, 0.0f, 1.0f), m_aabb.GetCenter(), Vec3f(0.0f, 1.0f, 0.0f)));

        InitObject(m_camera);

        CreateView();

        m_renderResource->SetViewResourceHandle(TResourceHandle<RenderView>(m_view->GetRenderResource()));
    }

    if (ShouldComputePrefilteredEnvMap())
    {
        if (!m_prefilteredEnvMap)
        {
            m_prefilteredEnvMap = CreateObject<Texture>(TextureDesc {
                TT_TEX2D,
                TF_RGBA16F,
                Vec3u { 1024, 1024, 1 },
                TFM_LINEAR_MIPMAP,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE,
                1,
                IU_STORAGE | IU_SAMPLED });

            Assert(InitObject(m_prefilteredEnvMap));

            m_prefilteredEnvMap->SetPersistentRenderResourceEnabled(true);
        }
    }

    EnvProbeShaderData bufferData {};
    bufferData.aabbMin = Vec4f(m_aabb.min, 1.0f);
    bufferData.aabbMax = Vec4f(m_aabb.max, 1.0f);
    bufferData.worldPosition = Vec4f(GetOrigin(), 1.0f);
    bufferData.cameraNear = m_cameraNear;
    bufferData.cameraFar = m_cameraFar;
    bufferData.dimensions = Vec2u { m_dimensions.x, m_dimensions.y };
    bufferData.visibilityBits = m_visibilityBits.ToUInt64();
    bufferData.flags = (IsReflectionProbe() ? EnvProbeFlags::PARALLAX_CORRECTED : EnvProbeFlags::NONE)
        | (IsShadowProbe() ? EnvProbeFlags::SHADOW : EnvProbeFlags::NONE)
        | EnvProbeFlags::DIRTY;

    m_renderResource->SetBufferData(bufferData);

    // TEMP! Need to keep it alive since for right now we are reading renderresource's BufferData - will change w/ new proxy binding system!
    m_renderResource->IncRef();

    SetReady(true);
}

void EnvProbe::OnAddedToWorld(World* world)
{
    // if (m_view.IsValid())
    // {
    //     world->AddView(m_view);
    // }
}

void EnvProbe::OnRemovedFromWorld(World* world)
{
    // if (m_view.IsValid())
    // {
    //     world->RemoveView(m_view);
    // }
}

void EnvProbe::OnAddedToScene(Scene* scene)
{
    if (m_view.IsValid())
    {
        m_view->AddScene(scene->HandleFromThis());
    }

    Invalidate();
}

void EnvProbe::OnRemovedFromScene(Scene* scene)
{
    if (m_view.IsValid())
    {
        m_view->RemoveScene(scene->HandleFromThis());
    }

    Invalidate();
}

void EnvProbe::CreateView()
{
    if (IsControlledByEnvGrid())
    {
        return;
    }

    ViewOutputTargetDesc outputTargetDesc {
        .extent = Vec2u(m_dimensions),
        .attachments = {},
        .numViews = 6
    };

    if (IsReflectionProbe() || IsSkyProbe())
    {
        outputTargetDesc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
            .format = reflectionProbeFormat,
            .imageType = TT_CUBEMAP,
            .loadOp = LoadOperation::CLEAR,
            .storeOp = StoreOperation::STORE });

        outputTargetDesc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
            .format = TF_RG16F,
            .imageType = TT_CUBEMAP,
            .loadOp = LoadOperation::CLEAR,
            .storeOp = StoreOperation::STORE });

        outputTargetDesc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
            .format = TF_RGBA16F,
            .imageType = TT_CUBEMAP,
            .loadOp = LoadOperation::CLEAR,
            .storeOp = StoreOperation::STORE,
            .clearColor = MathUtil::Infinity<Vec4f>() });
    }
    else if (IsShadowProbe())
    {
        outputTargetDesc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
            .format = shadowProbeFormat,
            .imageType = TT_CUBEMAP,
            .loadOp = LoadOperation::CLEAR,
            .storeOp = StoreOperation::STORE });
    }

    outputTargetDesc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
        .format = g_renderBackend->GetDefaultFormat(DIF_DEPTH),
        .imageType = TT_CUBEMAP,
        .loadOp = LoadOperation::CLEAR,
        .storeOp = StoreOperation::STORE });

    ViewDesc viewDesc {
        .flags = (OnlyCollectStaticEntities() ? ViewFlags::COLLECT_STATIC_ENTITIES : ViewFlags::COLLECT_ALL_ENTITIES)
            | ViewFlags::NO_FRUSTUM_CULLING
            | ViewFlags::SKIP_ENV_PROBES
            | ViewFlags::SKIP_ENV_GRIDS
            | ViewFlags::NOT_MULTI_BUFFERED,
        .viewport = Viewport { .extent = m_dimensions, .position = Vec2i::Zero() },
        .outputTargetDesc = outputTargetDesc,
        .scenes = {},
        .camera = m_camera,
        .overrideAttributes = RenderableAttributeSet(
            MeshAttributes {},
            MaterialAttributes {
                .shaderDefinition = m_renderResource->GetShader()->GetCompiledShader()->GetDefinition(),
                .blendFunction = BlendFunction::AlphaBlending(),
                .cullFaces = FCM_NONE })
    };

    m_view = CreateObject<View>(viewDesc);

    InitObject(m_view);

    // temp shit
    m_view->GetRenderResource().IncRef();
}

void EnvProbe::SetAABB(const BoundingBox& aabb)
{
    HYP_SCOPE;

    if (m_aabb != aabb)
    {
        m_aabb = aabb;

        Invalidate();
    }
}

void EnvProbe::SetOrigin(const Vec3f& origin)
{
    HYP_SCOPE;

    if (IsAmbientProbe())
    {
        // ambient probes use the min point of the aabb as the origin,
        // so it can blend between 7 other probes
        const Vec3f extent = m_aabb.GetExtent();

        m_aabb.SetMin(origin);
        m_aabb.SetMax(origin + extent);
    }
    else
    {
        m_aabb.SetCenter(origin);
    }

    Invalidate();
}

void EnvProbe::Update(float delta)
{
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    Assert(GetScene() != nullptr);

    const Octree& octree = GetScene()->GetOctree();

    // Ambient probes do not use their own render list,
    // instead they are attached to a grid which shares one
    if (!IsControlledByEnvGrid())
    {
        HashCode octantHashCode = HashCode(0);

        if (OnlyCollectStaticEntities())
        {
            Octree const* octant = &GetScene()->GetOctree();

            if (!IsSkyProbe())
            {
                octree.GetFittingOctant(m_aabb, octant);

                if (!octant)
                {
                    HYP_LOG(EnvProbe, Warning, "No containing octant found for EnvProbe {} with AABB: {}", Id(), m_aabb);
                }
            }

            if (octant)
            {
                octantHashCode = octant->GetOctantID().GetHashCode().Add(octant->GetEntryListHash<EntityTag::STATIC>()).Add(octant->GetEntryListHash<EntityTag::LIGHT>());
            }
        }
        else
        {
            octantHashCode = octree.GetOctantID().GetHashCode().Add(octree.GetEntryListHash<EntityTag::NONE>());
        }

        if (m_octantHashCode == octantHashCode && octantHashCode.Value() != 0)
        {
            return;
        }

        Assert(m_camera.IsValid());
        m_camera->Update(delta);

        m_view->UpdateVisibility();
        m_view->Collect();

        auto diff = m_view->GetLastMeshCollectionResult();

        if (diff.NeedsUpdate() || m_octantHashCode != octantHashCode)
        {
            HYP_LOG(EnvProbe, Debug, "EnvProbe {} with AABB: {} has {} added, {} removed and {} changed entities", Id(), m_aabb,
                diff.numAdded, diff.numRemoved, diff.numChanged);

            SetNeedsRender(true);
        }

        m_octantHashCode = octantHashCode;
    }

    EnvProbeShaderData bufferData {};
    bufferData.aabbMin = Vec4f(m_aabb.min, 1.0f);
    bufferData.aabbMax = Vec4f(m_aabb.max, 1.0f);
    bufferData.worldPosition = Vec4f(GetOrigin(), 1.0f);
    bufferData.cameraNear = m_cameraNear;
    bufferData.cameraFar = m_cameraFar;
    bufferData.dimensions = Vec2u { m_dimensions.x, m_dimensions.y };
    bufferData.visibilityBits = m_visibilityBits.ToUInt64();
    bufferData.flags = (IsReflectionProbe() ? EnvProbeFlags::PARALLAX_CORRECTED : EnvProbeFlags::NONE)
        | (IsShadowProbe() ? EnvProbeFlags::SHADOW : EnvProbeFlags::NONE)
        | EnvProbeFlags::DIRTY;

    m_renderResource->SetBufferData(bufferData);

    if (IsShadowProbe())
    {
        GetWorld()->GetRenderResource().IncRef();
        m_renderResource->IncRef();
        PUSH_RENDER_COMMAND(RenderPointLightShadow, &GetWorld()->GetRenderResource(), m_renderResource);
    }
}

void EnvProbe::UpdateRenderProxy(IRenderProxy* proxy)
{
    RenderProxyEnvProbe* proxyCasted = static_cast<RenderProxyEnvProbe*>(proxy);
    proxyCasted->envProbe = WeakHandleFromThis();

    EnvProbeShaderData& bufferData = proxyCasted->bufferData;
    bufferData.aabbMin = Vec4f(m_aabb.min, 1.0f);
    bufferData.aabbMax = Vec4f(m_aabb.max, 1.0f);
    bufferData.worldPosition = Vec4f(GetOrigin(), 1.0f);
    bufferData.cameraNear = m_cameraNear;
    bufferData.cameraFar = m_cameraFar;
    bufferData.dimensions = Vec2u { m_dimensions.x, m_dimensions.y };
    bufferData.visibilityBits = m_visibilityBits.ToUInt64();
    bufferData.flags = (IsReflectionProbe() ? EnvProbeFlags::PARALLAX_CORRECTED : EnvProbeFlags::NONE)
        | (IsShadowProbe() ? EnvProbeFlags::SHADOW : EnvProbeFlags::NONE)
        | EnvProbeFlags::DIRTY;

    const FixedArray<Matrix4, 6> viewMatrices = CreateCubemapMatrices(m_aabb, GetOrigin());

    Memory::MemCpy(bufferData.faceViewMatrices, viewMatrices.Data(), sizeof(EnvProbeShaderData::faceViewMatrices));
    // Memory::MemCpy(bufferData.sh.values, m_sphericalHarmonics.values, sizeof(EnvProbeSphericalHarmonics::values));

    // /// temp shit
    // if (IsShadowProbe())
    // {
    //     // Assert(m_shadowMap);

    //     // bufferData->textureIndex = m_shadowMap->GetAtlasElement().pointLightIndex;
    // }
    // else
    // {
    //     bufferData.textureIndex = m_renderResource->GetTextureSlot();
    // }

    bufferData.positionInGrid = m_positionInGrid;
}

#pragma region SkyProbe

void SkyProbe::Init()
{
    EnvProbe::Init();

    m_skyboxCubemap = CreateObject<Texture>(TextureDesc {
        TT_CUBEMAP,
        TF_R11G11B10F,
        Vec3u { m_dimensions.x, m_dimensions.y, 1 },
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR });

    InitObject(m_skyboxCubemap);
    m_skyboxCubemap->SetPersistentRenderResourceEnabled(true);
}

#pragma endregion SkyProbe

} // namespace hyperion
