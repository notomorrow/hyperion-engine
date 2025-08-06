/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/ByteBuffer.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/Variant.hpp>

#include <core/Defines.hpp>

#include <core/Types.hpp>

namespace hyperion {
namespace net {

struct SocketID
{
    int value;
};

enum SocketResultType
{
    SOCKET_RESULT_TYPE_NONE,
    SOCKET_RESULT_TYPE_ERROR,
    SOCKET_RESULT_TYPE_DATA,
    SOCKET_RESULT_TYPE_NO_DATA,
    SOCKET_RESULT_TYPE_DISCONNECTED
};

class SocketServer;
struct SocketServerImpl;

using SocketProcArgument = Variant<String, ByteBuffer, Name, int8, int16, int32, int64, uint8, uint16, uint32, uint64, float, double>;

class HYP_API SocketConnection
{
public:
    SocketConnection() = default;
    SocketConnection(const SocketConnection&) = delete;
    SocketConnection& operator=(const SocketConnection&) = delete;
    SocketConnection(SocketConnection&&) noexcept = delete;
    SocketConnection& operator=(SocketConnection&&) noexcept = delete;
    virtual ~SocketConnection() = default;

    void SetEventProc(Name eventName, Proc<void(Array<SocketProcArgument>&&)>&& proc)
    {
        m_eventProcs[eventName] = std::move(proc);
    }

    void TriggerProc(Name eventName, Array<SocketProcArgument>&& args);

protected:
    HashMap<Name, Proc<void(Array<SocketProcArgument>&&)>> m_eventProcs;
};

class HYP_API SocketServerThread final : public Thread<Scheduler, SocketServer*>
{
public:
    SocketServerThread(const String& socketName);
    virtual ~SocketServerThread() override = default;

private:
    virtual void operator()(SocketServer*) override;
};

class HYP_API SocketClient : public SocketConnection
{
public:
    SocketClient(Name name, SocketID internalId);
    SocketClient(const SocketClient&) = delete;
    SocketClient& operator=(const SocketClient&) = delete;
    SocketClient(SocketClient&&) noexcept = delete;
    SocketClient& operator=(SocketClient&&) noexcept = delete;
    virtual ~SocketClient() override = default;

    Name GetName() const
    {
        return m_name;
    }

    SocketResultType Send(const ByteBuffer& data);
    SocketResultType Receive(ByteBuffer& outData);

    void Close();

private:
    Name m_name;
    SocketID m_internalId;
};

class HYP_API SocketServer : public SocketConnection
{
public:
    friend class SocketServerThread;

    SocketServer(String name);
    SocketServer(const SocketServer&) = delete;
    SocketServer& operator=(const SocketServer&) = delete;
    SocketServer(SocketServer&&) noexcept = delete;
    SocketServer& operator=(SocketServer&&) noexcept = delete;
    virtual ~SocketServer() override;

    SocketResultType Send(Name clientName, const ByteBuffer& data);

    bool Start();
    bool Stop();

private:
    // for the thread
    bool PollForConnections(Array<RC<SocketClient>>& outConnections);

    // for the thread
    void AddConnection(RC<SocketClient>&& connection);
    bool RemoveConnection(Name clientName);

    String m_name;
    UniquePtr<SocketServerImpl> m_impl;
    UniquePtr<SocketServerThread> m_thread;

    // SocketServerThread controls the connections list
    HashMap<Name, RC<SocketClient>> m_connections;
    Mutex m_connectionsMutex;
};

} // namespace net

using net::SocketClient;
using net::SocketConnection;
using net::SocketID;
using net::SocketProcArgument;
using net::SocketResultType;
using net::SocketServer;
using net::SocketServerThread;

} // namespace hyperion
