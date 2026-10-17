/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/WebGPU/WebGPUShared.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Functional/Proc.hpp>

namespace Hyperion {

class WebGPUShaderInstance;

enum class WebGPUPipelineCompileMode : uint8
{
    /// The browser compiles it when it gets to it, holding up everything queued behind.
    Immediate,

    /// Created with the async call while the frame's recorded commands execute.
    /// Draws for pipeline are skipped.
    /// Replayed once ready.
    WithFrame,

    /// Queued, and created with the async call once nothing else is being compiled.
    /// Not replayed.
    InBackground
};

/// A pipeline for one set of bind group layouts. Heap allocated: a compile in progress holds on to it until it has
/// finished, whatever became of the pipeline object in the meantime.
template <class PipelineType>
struct WebGPUPipelineVariant
{
    uint64 key = 0;
    Name shaderName;
    PipelineType pipeline = nullptr;
    WebGPUPipelineCompileMode compileMode = WebGPUPipelineCompileMode::Immediate;
    bool isCompiling = false;
    bool isOrphaned = false;
};

class WebGPUPipelineCompiler final
{
public:
    /// \param canStartLate whether the caller copes with InBackground
    WebGPUPipelineCompileMode GetCompileMode(const WebGPUShaderInstance& shaderInstance, bool canStartLate) const;

    /// Brackets one execution of the frame's recorded commands.
    void BeginFramePass(bool allowCompilingWithFrame);
    void EndFramePass();

    HYP_FORCE_INLINE uint32 GetNumFrameCompilesStarted() const
    {
        return m_numFrameCompilesStarted;
    }

    HYP_FORCE_INLINE uint32 GetNumFrameCompilesRunning() const
    {
        return m_numFrameCompilesRunning;
    }

    /// Queued and running.
    BackgroundCompileStatus GetBackgroundCompileStatus(StringHash shaderName) const;

    void OnCompileStarted(WebGPUPipelineCompileMode compileMode, Name shaderName);
    void OnCompileFinished(WebGPUPipelineCompileMode compileMode, Name shaderName);
    void OnDispatchSkipped(Name shaderName);

    void QueueBackgroundCompile(Name shaderName, Proc<bool()>&& start);

    void Update();

private:
    bool m_isFramePassCompiling = false;
    uint32 m_numFrameCompilesStarted = 0;
    uint32 m_numFrameCompilesRunning = 0;

    struct QueuedBackgroundCompile
    {
        Name shaderName;
        Proc<bool()> start;
    };

    Array<QueuedBackgroundCompile, WebGPUAllocator> m_queuedBackgroundCompiles;

    Map<Name, BackgroundCompileStatus, WebGPUAllocator> m_backgroundCompiles;

    uint32 m_numBackgroundCompilesRunning = 0;
    uint32 m_numQuietFrames = 0;
};

} // namespace Hyperion
