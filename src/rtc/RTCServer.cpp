#include <rtc/RTCServer.hpp>

#include <core/lib/CMemory.hpp>

#ifdef HYP_LIBDATACHANNEL

#include <rtc/rtc.hpp>
#include <variant>

#else
// Stub it out

namespace rtc {
class WebSocket { };
} // namespace rtc

#endif // HYP_LIBDATACHANNEL

namespace hyperion::v2 {

RTCServer::RTCServer(RTCServerParams params)
    : m_params(std::move(params))
{
}

// null (stubbed) implementation

NullRTCServer::NullRTCServer(RTCServerParams params)
    : RTCServer(std::move(params))
{
}

void NullRTCServer::Start()
{
    AssertThrowMsg(!m_is_running, "NullRTCServer::Start() called, but server is already running!");

    m_is_running = true;

    m_callbacks.Trigger(RTCServerCallbackMessages::SERVER_STARTED, {
        Optional<ByteBuffer>(),
        Optional<RTCServerError>()
    });
}

void NullRTCServer::Stop()
{
    AssertThrowMsg(m_is_running, "NullRTCServer::Stop() called, but server is not running!");
    
    m_is_running = false;

    m_callbacks.Trigger(RTCServerCallbackMessages::SERVER_STOPPED, {
        Optional<ByteBuffer>(),
        Optional<RTCServerError>()
    });
}

void NullRTCServer::SendToSignallingServer(const ByteBuffer &bytes)
{
    AssertThrowMsg(m_is_running, "NullRTCServer::SendToSignallingServer() called, but server is not running!");

    // Do nothing
}


// libdatachannel implementation

LibDataChannelRTCServer::LibDataChannelRTCServer(RTCServerParams params)
    : RTCServer(std::move(params))
{
}

LibDataChannelRTCServer::~LibDataChannelRTCServer() = default;

void LibDataChannelRTCServer::Start()
{
    AssertThrowMsg(!m_is_running, "LibDataChannelRTCServer::Start() called, but server is already running!");

#ifdef HYP_LIBDATACHANNEL
    AssertThrowMsg(m_websocket == nullptr, "LibDataChannelRTCServer::Start() called, but m_websocket is not nullptr!");

    m_websocket.Reset(new rtc::WebSocket());

    m_websocket->onOpen([this]() {
        m_is_running = true;

        m_callbacks.Trigger(RTCServerCallbackMessages::SERVER_STARTED, {
            Optional<ByteBuffer>(),
            Optional<RTCServerError>()
        });
    });

    m_websocket->onClosed([this]() {
        m_is_running = false;

        m_callbacks.Trigger(RTCServerCallbackMessages::SERVER_STOPPED, {
            Optional<ByteBuffer>(),
            Optional<RTCServerError>()
        });
    });

    m_websocket->onError([this](const std::string &error) {
        m_callbacks.Trigger(RTCServerCallbackMessages::SERVER_ERROR, {
            Optional<ByteBuffer>(),
            Optional<RTCServerError>(RTCServerError { error.c_str() })
        });
    });

    m_websocket->onMessage([this](const std::variant<rtc::binary, std::string> &data) {
        if (std::holds_alternative<rtc::binary>(data)) {
            const rtc::binary &bytes = std::get<rtc::binary>(data);

            m_callbacks.Trigger(RTCServerCallbackMessages::CLIENT_MESSAGE, {
                Optional<ByteBuffer>(ByteBuffer(bytes.size(), bytes.data())),
                Optional<RTCServerError>()
            });
        } else {
            const std::string &str = std::get<std::string>(data);

            m_callbacks.Trigger(RTCServerCallbackMessages::CLIENT_MESSAGE, {
                Optional<ByteBuffer>(ByteBuffer(str.size(), str.data())),
                Optional<RTCServerError>()
            });
        }
    });

    const String websocket_url = m_params.address.host + ":" + String::ToString(m_params.address.port);

    DebugLog(LogType::Debug, "Attempting to connect websocket server to url: %s\n", websocket_url.Data());

    m_websocket->open(websocket_url.Data());
#else
    DebugLog(LogType::Error, "LibDataChannelRTCServer::Start() called, but HYP_LIBDATACHANNEL is not defined!");
    
    m_callbacks.Trigger(RTCServerCallbackMessages::SERVER_ERROR, {
        Optional<ByteBuffer>(),
        Optional<RTCServerError>(RTCServerError { "LibDataChannelRTCServer::Start() called, but HYP_LIBDATACHANNEL is not defined!" })
    });
#endif
}

void LibDataChannelRTCServer::Stop()
{
    AssertThrowMsg(m_is_running, "LibDataChannelRTCServer::Stop() called, but server is not running!");
    
#ifdef HYP_LIBDATACHANNEL
    AssertThrow(m_websocket != nullptr);
    AssertThrowMsg(m_websocket->isOpen(), "Expected websocket to be open");
    
    m_websocket->close();
#else
    m_is_running = false;

    m_callbacks.Trigger(RTCServerCallbackMessages::SERVER_STOPPED, {
        Optional<ByteBuffer>(),
        Optional<RTCServerError>()
    });
#endif
}

void LibDataChannelRTCServer::SendToSignallingServer(const ByteBuffer &bytes)
{
    AssertThrowMsg(m_is_running, "LibDataChannelRTCServer::SendToSignallingServer() called, but server is not running!");
 
#ifdef HYP_LIBDATACHANNEL
    AssertThrow(m_websocket != nullptr);
    AssertThrowMsg(m_websocket->isOpen(), "Expected websocket to be open");

    rtc::binary bin;
    bin.resize(bytes.Size());

    Memory::MemCpy(bin.data(), bytes.Data(), bytes.Size());

    m_websocket->send(std::move(bin));
#else
    DebugLog(LogType::Error, "LibDataChannelRTCServer::SendToSignallingServer() called, but HYP_LIBDATACHANNEL is not defined!");
#endif
}

} // namespace hyperion::v2