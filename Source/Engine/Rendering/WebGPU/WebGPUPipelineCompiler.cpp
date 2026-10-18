/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUPipelineCompiler.hpp>
#include <Rendering/WebGPU/WebGPUShaderInstance.hpp>

#include <Rendering/Shader.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {

static constexpr uint32 NumQuietFramesBeforeBackgroundCompile = 30;
static constexpr uint32 MaxBackgroundCompilesRunning = 3;

// These ray traversal shaders take the browser seconds each to compile.
static const ANSIStringView g_backgroundCompiledShaderPrefix = "Glimmer";

WebGPUPipelineCompileMode WebGPUPipelineCompiler::GetCompileMode(const WebGPUShaderInstance& shaderInstance, bool canStartLate) const
{
    const ANSIStringView shaderName = shaderInstance.GetShader()->GetName().LookupString();

    if (canStartLate
        && shaderName.Size() >= g_backgroundCompiledShaderPrefix.Size()
        && Memory::StrCmp(shaderName.Data(), g_backgroundCompiledShaderPrefix.Data(), g_backgroundCompiledShaderPrefix.Size()) == 0)
    {
        return WebGPUPipelineCompileMode::InBackground;
    }

    // transient command buffers are recorded on other threads while the frame executes, and are not executed again
    if (m_isFramePassCompiling && IsOnThread(g_renderThread))
    {
        return WebGPUPipelineCompileMode::WithFrame;
    }

    return WebGPUPipelineCompileMode::Immediate;
}

void WebGPUPipelineCompiler::BeginFramePass(bool allowCompilingWithFrame)
{
    AssertOnThread(g_renderThread);

    m_isFramePassCompiling = allowCompilingWithFrame;
    m_numFrameCompilesStarted = 0;
}

void WebGPUPipelineCompiler::EndFramePass()
{
    m_isFramePassCompiling = false;
}

BackgroundCompileStatus WebGPUPipelineCompiler::GetBackgroundCompileStatus(StringHash shaderName) const
{
    if (const auto* it = m_backgroundCompiles.TryGet(Name(shaderName)))
    {
        return it->second;
    }

    return {};
}

void WebGPUPipelineCompiler::OnCompileStarted(WebGPUPipelineCompileMode compileMode, Name shaderName)
{
    switch (compileMode)
    {
    case WebGPUPipelineCompileMode::WithFrame:
        ++m_numFrameCompilesStarted;
        ++m_numFrameCompilesRunning;

        m_numQuietFrames = 0;
        break;
    case WebGPUPipelineCompileMode::InBackground:
        ++m_backgroundCompiles[shaderName].numCompiling;
        break;
    default:
        m_numQuietFrames = 0;
        break;
    }
}

void WebGPUPipelineCompiler::OnCompileFinished(WebGPUPipelineCompileMode compileMode, Name shaderName)
{
    switch (compileMode)
    {
    case WebGPUPipelineCompileMode::WithFrame:
        AssertDebug(m_numFrameCompilesRunning != 0);
        --m_numFrameCompilesRunning;
        break;
    case WebGPUPipelineCompileMode::InBackground:
    {
        BackgroundCompileStatus& status = m_backgroundCompiles[shaderName];
        AssertDebug(status.numCompiling != 0);
        --status.numCompiling;

        AssertDebug(m_numBackgroundCompilesRunning != 0);
        --m_numBackgroundCompilesRunning;
        break;
    }
    default:
        break;
    }
}

void WebGPUPipelineCompiler::OnDispatchSkipped(Name shaderName)
{
    ++m_backgroundCompiles[shaderName].numSkippedDispatches;
}

void WebGPUPipelineCompiler::QueueBackgroundCompile(Name shaderName, Proc<bool()>&& start)
{
    m_queuedBackgroundCompiles.PushBack(QueuedBackgroundCompile { shaderName, std::move(start) });
}

void WebGPUPipelineCompiler::Update()
{
    ++m_numQuietFrames;

    while (m_numBackgroundCompilesRunning < MaxBackgroundCompilesRunning
        && m_numFrameCompilesRunning == 0
        && m_numQuietFrames >= NumQuietFramesBeforeBackgroundCompile
        && m_queuedBackgroundCompiles.Any())
    {
        QueuedBackgroundCompile queued = m_queuedBackgroundCompiles.PopFront();

        ++m_numBackgroundCompilesRunning;

        if (!queued.start())
        {
            OnCompileFinished(WebGPUPipelineCompileMode::InBackground, queued.shaderName);
        }
    }
}

} // namespace Hyperion
