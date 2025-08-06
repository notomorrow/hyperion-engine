/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#if 0

#include <core/Defines.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/containers/String.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/Types.hpp>

namespace hyperion {
namespace net {

class WebSocket;

class HYP_API WebSocketThread final : public Thread<Scheduler, WebSocket *>
{
public:
    WebSocketThread();
    virtual ~WebSocketThread() override = default;

    void Stop();

    /*! \brief Atomically load the boolean value indicating that this thread is actively running */
    bool IsRunning() const
        { return m_isRunning.Get(MemoryOrder::RELAXED); }

private:
    virtual void operator()(WebSocket *) override;

    AtomicVar<bool> m_isRunning;
};

class HYP_API WebSocket
{
public:
    WebSocket(const String &url);
    WebSocket(const WebSocket &other)               = delete;
    WebSocket &operator=(const WebSocket &other)    = delete;
    WebSocket(WebSocket &&other) noexcept;
    WebSocket &operator=(WebSocket &&other) noexcept;
    ~WebSocket();

    HYP_FORCE_INLINE const String &GetURL() const
        { return m_url; }

private:
    void WebSocketThreadProc();

    String                      m_url;
    UniquePtr<WebSocketThread>  m_thread;
};

} // namespace net

using net::WebSocket;
using net::WebSocketThread;

} // namespace hyperion

#endif
