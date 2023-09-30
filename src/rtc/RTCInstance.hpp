#ifndef HYPERION_V2_RTC_INSTANCE_HPP
#define HYPERION_V2_RTC_INSTANCE_HPP

#include <rtc/RTCClientList.hpp>
#include <rtc/RTCServer.hpp>

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

    RTCClientList &GetClientList()
        { return m_client_list; }

    const RTCClientList &GetClientList() const
        { return m_client_list; }

    RTCServer *GetServer() const
        { return m_server.Get(); }

private:
    RTCClientList           m_client_list;
    UniquePtr<RTCServer>    m_server;
};

} // namespace hyperion::v2

#endif