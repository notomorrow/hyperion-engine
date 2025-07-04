/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UISubsystem.hpp>
#include <ui/UIStage.hpp>

#include <scene/World.hpp>
#include <scene/Node.hpp>
#include <scene/Scene.hpp>

#include <scene/ecs/EntityManager.hpp>

#include <scene/ecs/components/MeshComponent.hpp>

#include <rendering/UIRenderer.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvironment.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

UISubsystem::UISubsystem(const Handle<UIStage>& uiStage)
    : m_uiStage(uiStage)
{
}

UISubsystem::~UISubsystem()
{
}

void UISubsystem::OnAddedToWorld()
{
    HYP_SCOPE;

    Assert(m_uiStage != nullptr);

    InitObject(m_uiStage);

    m_uiRenderSubsystem = GetWorld()->AddSubsystem(CreateObject<UIRenderSubsystem>(m_uiStage));
}

void UISubsystem::OnRemovedFromWorld()
{
    HYP_SCOPE;

    if (m_uiRenderSubsystem != nullptr)
    {
        GetWorld()->RemoveSubsystem(m_uiRenderSubsystem);
    }
}

void UISubsystem::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);

    Assert(m_uiStage != nullptr);
    Assert(m_uiRenderSubsystem != nullptr);
}

void UISubsystem::OnSceneAttached(const Handle<Scene>& scene)
{
}

void UISubsystem::OnSceneDetached(const Handle<Scene>& scene)
{
}

} // namespace hyperion
