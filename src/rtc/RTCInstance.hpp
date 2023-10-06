#ifndef HYPERION_V2_RTC_INSTANCE_HPP
#define HYPERION_V2_RTC_INSTANCE_HPP

#include <rtc/RTCClientList.hpp>
#include <rtc/RTCServer.hpp>

#include <core/lib/RefCountedPtr.hpp>

namespace hyperion::v2 {

class RTCInstance
{
public:
    RTCInstance(RTCServerParams server_params);
    RTCInstance(const RTCInstance &other) = delete;
    RTCInstance &operator=(const RTCInstance &other) = delete;
    RTCInstance(RTCInstance &&other) = delete;
    RTCInstance &operator=(RTCInstance &&other) = delete;
    ~RTCInstance() = default;

    const RC<RTCServer> &GetServer() const
        { return m_server; }

private:
    RC<RTCServer> m_server;
};

} // namespace hyperion::v2

#endif