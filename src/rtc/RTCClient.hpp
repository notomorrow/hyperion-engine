#ifndef HYPERION_V2_RTC_CLIENT_HPP
#define HYPERION_V2_RTC_CLIENT_HPP

#include <core/lib/String.hpp>
#include <core/lib/RefCountedPtr.hpp>

#ifdef HYP_LIBDATACHANNEL
namespace rtc {
class PeerConnection;
class DataChannel;
} // namespace rtc

#include <memory>

#endif

namespace hyperion::v2 {

class RTCServer;

class RTCClient
{
public:
    RTCClient(String id, RTCServer *server)
        : m_id(std::move(id)),
          m_server(server)
    {
    }

    RTCClient(const RTCClient &other) = delete;
    RTCClient &operator=(const RTCClient &other) = delete;
    RTCClient(RTCClient &&other) noexcept = default;
    RTCClient &operator=(RTCClient &&other) noexcept = default;
    virtual ~RTCClient() = default;

    virtual void InitPeerConnection() = 0;
    virtual void SetRemoteDescription(const String &type, const String &sdp) = 0;

protected:
    String      m_id;
    RTCServer   *m_server;
};

class NullRTCClient : public RTCClient
{
public:
    NullRTCClient(String id, RTCServer *server);
    NullRTCClient(const NullRTCClient &other) = delete;
    NullRTCClient &operator=(const NullRTCClient &other) = delete;
    NullRTCClient(NullRTCClient &&other) noexcept = default;
    NullRTCClient &operator=(NullRTCClient &&other) noexcept = default;
    virtual ~NullRTCClient() override = default;

    virtual void InitPeerConnection() override;
    virtual void SetRemoteDescription(const String &type, const String &sdp) override;
};

#ifdef HYP_LIBDATACHANNEL

class LibDataChannelRTCClient : public RTCClient
{
public:
    LibDataChannelRTCClient(String id, RTCServer *server);
    LibDataChannelRTCClient(const LibDataChannelRTCClient &other) = delete;
    LibDataChannelRTCClient &operator=(const LibDataChannelRTCClient &other) = delete;
    LibDataChannelRTCClient(LibDataChannelRTCClient &&other) noexcept = default;
    LibDataChannelRTCClient &operator=(LibDataChannelRTCClient &&other) noexcept = default;
    virtual ~LibDataChannelRTCClient() override = default;

    virtual void InitPeerConnection() override;
    virtual void SetRemoteDescription(const String &type, const String &sdp) override;

private:
    RC<rtc::PeerConnection>             m_peer_connection;
    std::shared_ptr<rtc::DataChannel>   m_data_channel;
};

#else

using LibDataChannelRTCClient = NullRTCClient;

#endif // HYP_LIBDATACHANNEL

} // namespace hyperion::v2

#endif