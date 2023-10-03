#ifndef HYPERION_V2_RTC_SERVER_HPP
#define HYPERION_V2_RTC_SERVER_HPP

#include <core/Containers.hpp>
#include <core/lib/ByteBuffer.hpp>
#include <core/lib/Optional.hpp>

namespace rtc {
class WebSocket;
} // namespace rtc

namespace hyperion::v2 {

enum class RTCServerCallbackMessages : UInt32
{
    UNKNOWN         = 0,

    SERVER_ERROR    = UInt32(-1),

    SERVER_STARTED  = 1,
    SERVER_STOPPED,

    CLIENT_CONNECTED,
    CLIENT_DISCONNECTED,
    CLIENT_MESSAGE
};

struct RTCServerError
{
    String message;
};

struct RTCServerCallbackData
{
    Optional<ByteBuffer>        bytes;
    Optional<RTCServerError>    error;
};

using RTCServerCallbacks = Callbacks<RTCServerCallbackMessages, RTCServerCallbackData>;

struct RTCServerAddress
{
    String host;
    UInt16 port;
};

struct RTCServerParams
{
    RTCServerAddress address;
};

class RTCServer
{
public:
    RTCServer(RTCServerParams params);
    RTCServer(const RTCServer &other)               = delete;
    RTCServer &operator=(const RTCServer &other)    = delete;
    RTCServer(RTCServer &&other)                    = delete;
    RTCServer &operator=(RTCServer &&other)         = delete;
    virtual ~RTCServer()                            = default;

    virtual void Start() = 0;
    virtual void Stop() = 0;

    virtual void SendToSignallingServer(const ByteBuffer &bytes) = 0;

    const RTCServerParams &GetParams() const
        { return m_params; }

    RTCServerCallbacks &GetCallbacks()
        { return m_callbacks; }

    const RTCServerCallbacks &GetCallbacks() const
        { return m_callbacks; }

protected:
    RTCServerParams     m_params;
    RTCServerCallbacks  m_callbacks;
};

class NullRTCServer : public RTCServer
{
public:
    NullRTCServer(RTCServerParams params);
    NullRTCServer(const NullRTCServer &other)               = delete;
    NullRTCServer &operator=(const NullRTCServer &other)    = delete;
    NullRTCServer(NullRTCServer &&other)                    = delete;
    NullRTCServer &operator=(NullRTCServer &&other)         = delete;
    virtual ~NullRTCServer()                                = default;

    virtual void Start() override;
    virtual void Stop() override;

    virtual void SendToSignallingServer(const ByteBuffer &bytes) override;

private:
    bool m_is_running = false;
};

class LibDataChannelRTCServer : public RTCServer
{
public:
    LibDataChannelRTCServer(RTCServerParams params);
    LibDataChannelRTCServer(const LibDataChannelRTCServer &other)               = delete;
    LibDataChannelRTCServer &operator=(const LibDataChannelRTCServer &other)    = delete;
    LibDataChannelRTCServer(LibDataChannelRTCServer &&other)                    = delete;
    LibDataChannelRTCServer &operator=(LibDataChannelRTCServer &&other)         = delete;
    virtual ~LibDataChannelRTCServer() override;

    virtual void Start() override;
    virtual void Stop() override;

    virtual void SendToSignallingServer(const ByteBuffer &bytes) override;

private:
    bool                        m_is_running = false;
    UniquePtr<rtc::WebSocket>   m_websocket;
};

} // namespace hyperion::v2

#endif