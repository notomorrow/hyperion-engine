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

#include <Framework/View.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Util/Img/WritePng.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

namespace EditorHelpers {

namespace {

inline ubyte LinearToSrgb8(float value)
{
    value = MathUtil::Clamp(value, 0.0f, 1.0f);
    value = value <= 0.0031308f
        ? value * 12.92f
        : 1.055f * MathUtil::Pow(value, 1.0f / 2.4f) - 0.055f;

    return ubyte(MathUtil::Clamp(value * 255.0f + 0.5f, 0.0f, 255.0f));
}

ByteBuffer ResampleToSrgb8(const Float16* src, Vec2u srcExtent, Vec2u dstExtent)
{
    uint32 cropWidth = srcExtent.x;
    uint32 cropHeight = srcExtent.y;

    if (uint64(srcExtent.x) * dstExtent.y > uint64(dstExtent.x) * srcExtent.y)
    {
        cropWidth = MathUtil::Max(uint32(uint64(srcExtent.y) * dstExtent.x / dstExtent.y), 1u);
    }
    else
    {
        cropHeight = MathUtil::Max(uint32(uint64(srcExtent.x) * dstExtent.y / dstExtent.x), 1u);
    }

    const uint32 offsetX = (srcExtent.x - cropWidth) / 2;
    const uint32 offsetY = (srcExtent.y - cropHeight) / 2;

    ByteBuffer rgba8(size_t(dstExtent.x) * dstExtent.y * 4);
    ubyte* dst = rgba8.Data();

    for (uint32 y = 0; y < dstExtent.y; y++)
    {
        const uint32 y0 = offsetY + uint32(uint64(y) * cropHeight / dstExtent.y);
        const uint32 y1 = MathUtil::Max(offsetY + uint32(uint64(y + 1) * cropHeight / dstExtent.y), y0 + 1);

        for (uint32 x = 0; x < dstExtent.x; x++)
        {
            const uint32 x0 = offsetX + uint32(uint64(x) * cropWidth / dstExtent.x);
            const uint32 x1 = MathUtil::Max(offsetX + uint32(uint64(x + 1) * cropWidth / dstExtent.x), x0 + 1);

            float sum[3] = { 0.0f, 0.0f, 0.0f };

            for (uint32 sy = y0; sy < y1; sy++)
            {
                for (uint32 sx = x0; sx < x1; sx++)
                {
                    const Float16* pixel = src + (size_t(sy) * srcExtent.x + sx) * 4;

                    for (uint32 c = 0; c < 3; c++)
                    {
                        sum[c] += MathUtil::Clamp(float(pixel[c]), 0.0f, 1.0f);
                    }
                }
            }

            const float invCount = 1.0f / float((x1 - x0) * (y1 - y0));
            ubyte* out = dst + (size_t(y) * dstExtent.x + x) * 4;

            for (uint32 c = 0; c < 3; c++)
            {
                out[c] = LinearToSrgb8(sum[c] * invCount);
            }

            out[3] = 255;
        }
    }

    return rgba8;
}

} // namespace

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
                        const ByteBuffer rgba8 = ResampleToSrgb8(reinterpret_cast<const Float16*>(color.Data()), extent, dstExtent);

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
