/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorHelpers.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/Time.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Logging/Logger.hpp>

#include <Rendering/ThumbnailCaptureState.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/Util/TextureUtils.hpp>

#include <Framework/View.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Util/Img/WritePng.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

namespace EditorHelpers {

Task<void> CaptureViewToPNGAsync(
    const Handle<View>& view,
    const FilePath& path,
    const Vec2u& wireframeExtent,
    const Vec2u& outputExtent)
{
    if (!view.IsValid())
    {
        return {};
    }

    Task<void> future;
    TaskPromise<void>* promise = future.Promise();

    GetThreadById(g_renderThread)->GetScheduler().Enqueue(
        [view, path, wireframeExtent, outputExtent, promise]()
        {
            static constexpr uint64 CaptureTimeoutMs = 5000;

            static ThumbnailCaptureState* s_pendingCapture = nullptr;
            static uint64 s_pendingSinceMs = 0;

            if (view->thumbnailCaptureState != nullptr)
            {
                if (view->thumbnailCaptureState != s_pendingCapture || Time::Now().ToMilliseconds() - s_pendingSinceMs < CaptureTimeoutMs)
                {
                    HYP_LOG(Editor, Warning, "a capture of this viewport is already in flight");

                    promise->Fulfill();

                    return;
                }

                HYP_LOG(Editor, Warning, "abandoning a viewport capture that never completed");
                view->thumbnailCaptureState = nullptr;
            }

            const bool isWireframeView = bool(view->GetFlags() & ViewFlags::EDITOR_WIREFRAME_VIEW);

            const FramebufferRef& framebuffer = !isWireframeView
                ? view->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque)
                : FramebufferRef::Null();

            if (!isWireframeView && !framebuffer.IsValid())
            {
                HYP_LOG(Editor, Warning, "the viewport hasn't rendered yet");

                promise->Fulfill();

                return;
            }

            ThumbnailCaptureState* captureState = new ThumbnailCaptureState(isWireframeView
                ? wireframeExtent
                : framebuffer->GetExtent());

            view->thumbnailCaptureState = captureState;

            s_pendingCapture = captureState;
            s_pendingSinceMs = Time::Now().ToMilliseconds();

            captureState->Request(
                [view, path, outputExtent, captureState, promise](ByteBuffer&& color, ByteBuffer&&, Vec2u extent)
                {
                    const Vec2u dstExtent = outputExtent.x != 0 && outputExtent.y != 0 ? outputExtent : extent;

                    // RGBA16F
                    if (extent.x != 0 && extent.y != 0 && color.Size() >= size_t(extent.x) * extent.y * 8)
                    {
                        const ByteBuffer rgba8 = TextureUtils::ResampleToSRGB8(reinterpret_cast<const Float16*>(color.Data()), extent, dstExtent);

                        path.BasePath().MkDir();

                        if (WritePng::Write(path, dstExtent.x, dstExtent.y, 4, rgba8.Data()))
                        {
                            HYP_LOG(Editor, Info, "Wrote viewport capture {} ({}x{})", path, dstExtent.x, dstExtent.y);
                        }
                        else
                        {
                            HYP_LOG(Editor, Error, "Failed to write viewport capture {}", path);
                        }
                    }

                    if (view->thumbnailCaptureState == captureState)
                    {
                        view->thumbnailCaptureState = nullptr;
                    }

                    promise->Fulfill();

                    // not from inside its own callback
                    GetThreadById(g_renderThread)->GetScheduler().Enqueue(
                        [captureState]()
                        {
                            delete captureState;
                        },
                        TaskEnqueueFlags::FIRE_AND_FORGET);
                });
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);

    return future;
}

} // namespace EditorHelpers
} // namespace Hyperion
