/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUFrame.hpp>
#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>
#include <Rendering/WebGPU/WebGPUPipelineCompiler.hpp>
#include <Rendering/WebGPU/WebGPUFramebuffer.hpp>

#include <WebGPUFrame.generated.inl>

namespace Hyperion {

extern WebGPURenderInterface RI;

WebGPUFrame::WebGPUFrame()
    : FrameBase(0)
{
}

WebGPUFrame::WebGPUFrame(uint32 frameIndex)
    : FrameBase(frameIndex)
{
}

WebGPUFrame::~WebGPUFrame()
{
}

bool WebGPUFrame::IsCreated() const
{
    return true;
}

RendererResult WebGPUFrame::Create()
{
    return {};
}

void WebGPUFrame::OnFrameStart()
{
    FrameBase::OnFrameStart();
}

void WebGPUFrame::WriteCommandBuffer(CommandBuffer* commandBuffer)
{
    AssertOnThread(g_renderThread);

    CommandRecorder* commandRecorders[] = {
        &preRenderCommands,
        &RI.commandRecorderAllocator.rootPreRender,
        &cr,
        &RI.commandRecorderAllocator.root,
        &postRenderCommands
    };

    if (OnPresent.AnyBound())
    {
        OnPresent(this);
        OnPresent.RemoveAllDetached();
    }

    // A pass that had to start compiling pipelines executed no draws so once they are in, commands are replayed.
    // The last pass allowed compiles as normal, which leaves nothing out.
    static constexpr uint32 MaxNumPasses = 3;

    WebGPUPipelineCompiler& pipelineCompiler = RI.GetPipelineCompiler();

    const WebGPUCommandBuffer::SectionMark sectionMark = commandBuffer->BeginDiscardableSection();
    const RenderInterface::State stateBefore = RI.state;

    for (uint32 passIndex = 0; passIndex < MaxNumPasses; passIndex++)
    {
        pipelineCompiler.BeginFramePass(/* allowCompilingWithFrame */ passIndex + 1 != MaxNumPasses);

        for (CommandRecorder* commandRecorder : commandRecorders)
        {
            // the custom commands hand payloads over to the end of the frame, once
            commandRecorder->Execute(commandBuffer, /* keepCommands */ true, /* skipCustomCommands */ passIndex != 0);
        }

        pipelineCompiler.EndFramePass();

        if (pipelineCompiler.GetNumFrameCompilesStarted() == 0)
        {
            break;
        }

        while (pipelineCompiler.GetNumFrameCompilesRunning() != 0 && !RI.IsDeviceLost())
        {
            RI.WaitForEvents();
        }

        if (RI.state.boundFramebuffer != nullptr)
        {
            RI.state.boundFramebuffer->EndCapture(commandBuffer);
        }

        commandBuffer->DiscardSection(sectionMark);

        RI.state = stateBefore;
    }

    for (CommandRecorder* commandRecorder : commandRecorders)
    {
        commandRecorder->Reset(/* freeMemory */ false);
    }
}

void WebGPUFrame::ResetTransientStates()
{
}

} // namespace Hyperion
