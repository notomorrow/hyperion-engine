#ifndef HYPERION_SAMPLE_STREAMER_HPP
#define HYPERION_SAMPLE_STREAMER_HPP

#include <Game.hpp>
#include <HyperionEngine.hpp>

using namespace hyperion;
using namespace hyperion::v2;

class SampleStreamer : public Game
{
public:
    SampleStreamer(RC<Application> application);
    virtual ~SampleStreamer() = default;

    virtual void InitGame() override;
    virtual void InitRender() override;

    virtual void Teardown() override;
    virtual void Logic(GameCounter::TickUnit delta) override;
    virtual void OnInputEvent(const SystemEvent &event) override;

    virtual void OnFrameEnd(Frame *frame) override;

private:
    void HandleCameraMovement(GameCounter::TickUnit delta);

    Handle<Texture> m_texture;
    ByteBuffer      m_screen_buffer;

    UInt            m_counter = 0;
};

#endif