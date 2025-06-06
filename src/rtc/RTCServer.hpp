/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_RTC_SERVER_HPP
#define HYPERION_RTC_SERVER_HPP

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/utilities/Optional.hpp>
#include <core/functional/Delegate.hpp>

#include <rtc/RTCClientList.hpp>

namespace rtc {
class WebSocket;
} // namespace rtc

namespace hyperion {

class RTCClient;
class RTCServerThread;

struct RTCServerError
{
    String message;
};

struct RTCServerCallbackData
{
    Optional<ByteBuffer> bytes;
    Optional<RTCServerError> error;
};

struct RTCServerCallbacks
{
    Delegate<void, RTCServerCallbackData&> OnError;
    Delegate<void, RTCServerCallbackData&> OnConnected;
    Delegate<void, RTCServerCallbackData&> OnDisconnected;
    Delegate<void, RTCServerCallbackData&> OnMessage;
};

struct RTCServerAddress
{
    String host;
    uint16 port;
    String path;
};

struct RTCServerParams
{
    RTCServerAddress address;
};

class HYP_API RTCServer
{
public:
    RTCServer(RTCServerParams params);
    RTCServer(const RTCServer& other) = delete;
    RTCServer& operator=(const RTCServer& other) = delete;
    RTCServer(RTCServer&& other) = delete;
    RTCServer& operator=(RTCServer&& other) = delete;
    virtual ~RTCServer();

    virtual void Start() = 0;
    virtual void Stop() = 0;

    virtual RC<RTCClient> CreateClient(String id) = 0;

    void EnqueueClientRemoval(String client_id);

    virtual void SendToSignallingServer(ByteBuffer bytes) = 0;
    virtual void SendToClient(String client_id, const ByteBuffer& bytes) = 0;

    const RTCServerParams& GetParams() const
    {
        return m_params;
    }

    RTCServerCallbacks& GetCallbacks()
    {
        return m_callbacks;
    }

    const RTCServerCallbacks& GetCallbacks() const
    {
        return m_callbacks;
    }

    RTCClientList& GetClientList()
    {
        return m_client_list;
    }

    const RTCClientList& GetClientList() const
    {
        return m_client_list;
    }

protected:
    RTCServerParams m_params;
    RTCServerCallbacks m_callbacks;
    RTCClientList m_client_list;
    UniquePtr<RTCServerThread> m_thread;
};

class HYP_API NullRTCServer : public RTCServer
{
public:
    NullRTCServer(RTCServerParams params);
    NullRTCServer(const NullRTCServer& other) = delete;
    NullRTCServer& operator=(const NullRTCServer& other) = delete;
    NullRTCServer(NullRTCServer&& other) = delete;
    NullRTCServer& operator=(NullRTCServer&& other) = delete;
    virtual ~NullRTCServer() override = default;

    virtual void Start() override;
    virtual void Stop() override;

    virtual RC<RTCClient> CreateClient(String id) override;

    virtual void SendToSignallingServer(ByteBuffer bytes) override;
    virtual void SendToClient(String client_id, const ByteBuffer& bytes) override;
};

#ifdef HYP_LIBDATACHANNEL

class HYP_API LibDataChannelRTCServer : public RTCServer
{
public:
    LibDataChannelRTCServer(RTCServerParams params);
    LibDataChannelRTCServer(const LibDataChannelRTCServer& other) = delete;
    LibDataChannelRTCServer& operator=(const LibDataChannelRTCServer& other) = delete;
    LibDataChannelRTCServer(LibDataChannelRTCServer&& other) = delete;
    LibDataChannelRTCServer& operator=(LibDataChannelRTCServer&& other) = delete;
    virtual ~LibDataChannelRTCServer() override;

    virtual void Start() override;
    virtual void Stop() override;

    virtual RC<RTCClient> CreateClient(String id) override;

    virtual void SendToSignallingServer(ByteBuffer bytes) override;
    virtual void SendToClient(String client_id, const ByteBuffer& bytes) override;

private:
    UniquePtr<rtc::WebSocket> m_websocket;
};

#else

using LibDataChannelRTCServer = NullRTCServer;

#endif // HYP_LIBDATACHANNEL

} // namespace hyperion

#endif