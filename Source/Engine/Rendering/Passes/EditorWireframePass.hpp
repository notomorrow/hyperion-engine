/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Map.hpp>
#include <Core/Reflection/Handle.hpp>
#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Vector4.hpp>
#include <Core/HashCode.hpp>

#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class Mesh;
class View;
struct RenderSetup;

/*! \brief Draws editor ortho views as lines over a grid. A view is only redrawn when it was asked to,
 *  or when its camera, meshes, debug draws or the grid changed; otherwise the last image is reused. */
class EditorWireframePass final
{
public:
    struct RenderResult
    {
        GpuImageViewRef imageView;
        bool isUpdated = false;
    };

    EditorWireframePass();
    EditorWireframePass(const EditorWireframePass& other) = delete;
    EditorWireframePass& operator=(const EditorWireframePass& other) = delete;
    ~EditorWireframePass();

    /*! \brief The image to composite for rs.view at rs.viewport, null if it can't be drawn yet. */
    RenderResult Render(Frame* frame, const RenderSetup& rs);

private:
    struct ViewTarget
    {
        FramebufferRef framebuffer;
        uint32 lastUsedFrame = 0;

        bool hasContent = false;
        Mat4f viewProjMat;
        Vec4f gridParams;
        HashCode debugDrawHash;
    };

    ViewTarget& GetOrCreateTarget(View* view, const Vec2u& extent);
    void RenderGrid(Frame* frame, const RenderSetup& rs);
    void PurgeUnusedTargets();

    Map<uint32, ViewTarget> m_targets;
    Handle<Mesh> m_quadMesh;
};

} // namespace Hyperion
