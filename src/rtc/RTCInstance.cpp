#include <rtc/RTCInstance.hpp>

namespace hyperion::v2 {

RTCInstance::RTCInstance(RTCServerParams server_params)
{
#ifdef HYP_LIBDATACHANNEL
    m_server.Reset(new LibDataChannelRTCServer(std::move(server_params)));
#else
    m_server.Reset(new NullRTCServer(std::move(server_params)));
#endif // HYP_LIBDATACHANNEL
}

} // namespace hyperion::v2