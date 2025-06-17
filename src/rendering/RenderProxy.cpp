#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderSkeleton.hpp>

#include <scene/Entity.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

extern HYP_API SafeDeleter* g_safeDeleter;

#pragma region RenderProxy

void RenderProxyMesh::SafeRelease()
{
    g_safeDeleter->SafeRelease(std::move(mesh));
    g_safeDeleter->SafeRelease(std::move(material));
    g_safeDeleter->SafeRelease(std::move(skeleton));
}

void RenderProxyMesh::IncRefs() const
{
    if (material.IsValid())
    {
        material->GetRenderResource().IncRef();
    }

    if (mesh.IsValid())
    {
        mesh->GetRenderResource().IncRef();
    }

    if (skeleton.IsValid())
    {
        skeleton->GetRenderResource().IncRef();
    }
}

void RenderProxyMesh::DecRefs() const
{
    if (material.IsValid())
    {
        material->GetRenderResource().DecRef();
    }

    if (mesh.IsValid())
    {
        mesh->GetRenderResource().DecRef();
    }

    if (skeleton.IsValid())
    {
        skeleton->GetRenderResource().DecRef();
    }
}

#pragma endregion RenderProxy

} // namespace hyperion