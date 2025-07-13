#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderProxy.hpp>

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

#pragma endregion RenderProxy

} // namespace hyperion