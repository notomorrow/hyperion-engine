#ifndef HYPERION_V2_RTC_CLIENT_HPP
#define HYPERION_V2_RTC_CLIENT_HPP

#include <core/lib/String.hpp>

namespace hyperion::v2 {

class RTCClient
{
public:
    RTCClient() = default;
    RTCClient(const RTCClient &other) = delete;
    RTCClient &operator=(const RTCClient &other) = delete;
    RTCClient(RTCClient &&other) = default;
    RTCClient &operator=(RTCClient &&other) = default;
    virtual ~RTCClient() = default;

private:
    String m_id;
};

} // namespace hyperion::v2

#endif