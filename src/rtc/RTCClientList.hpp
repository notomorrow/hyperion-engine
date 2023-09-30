#ifndef HYPERION_V2_RTC_CLIENT_LIST_HPP
#define HYPERION_V2_RTC_CLIENT_LIST_HPP

#include <rtc/RTCClient.hpp>

#include <core/lib/String.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/RefCountedPtr.hpp>

namespace hyperion::v2 {

class RTCClientList
{
public:
    RTCClientList() = default;
    RTCClientList(const RTCClientList &other) = delete;
    RTCClientList &operator=(const RTCClientList &other) = delete;
    RTCClientList(RTCClientList &&other) = default;
    RTCClientList &operator=(RTCClientList &&other) = default;
    ~RTCClientList() = default;

    RTCClient *GetClient(const String &id) const
    {
        const auto it = m_clients.Find(id);

        if (it == m_clients.End()) {
            return nullptr;
        }

        return it->value.Get();
    }

private:
    HashMap<String, RC<RTCClient>> m_clients;
};

} // namespace hyperion::v2

#endif