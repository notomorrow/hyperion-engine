#ifndef HYPERION_V2_RTC_STREAM_HPP
#define HYPERION_V2_RTC_STREAM_HPP

#include <core/lib/String.hpp>
#include <core/lib/RefCountedPtr.hpp>

namespace hyperion::v2 {

class RTCStreamVideoEncoder
{
public:
    RTCStreamVideoEncoder()                                                     = default;
    RTCStreamVideoEncoder(const RTCStreamVideoEncoder &other)                   = delete;
    RTCStreamVideoEncoder &operator=(const RTCStreamVideoEncoder &other)        = delete;
    RTCStreamVideoEncoder(RTCStreamVideoEncoder &&other) noexcept               = default;
    RTCStreamVideoEncoder &operator=(RTCStreamVideoEncoder &&other) noexcept    = default;
    virtual ~RTCStreamVideoEncoder()                                            = default;
};

class NullRTCStreamVideoEncoder : public RTCStreamVideoEncoder
{
public:
    NullRTCStreamVideoEncoder()                                                         = default;
    NullRTCStreamVideoEncoder(const NullRTCStreamVideoEncoder &other)                   = delete;
    NullRTCStreamVideoEncoder &operator=(const NullRTCStreamVideoEncoder &other)        = delete;
    NullRTCStreamVideoEncoder(NullRTCStreamVideoEncoder &&other) noexcept               = default;
    NullRTCStreamVideoEncoder &operator=(NullRTCStreamVideoEncoder &&other) noexcept    = default;
    virtual ~NullRTCStreamVideoEncoder()                                                = default;
};

#ifdef HYP_GSTREAMER

class GStreamerRTCStreamVideoEncoder : public RTCStreamVideoEncoder
{
public:
    GStreamerRTCStreamVideoEncoder()                                                            = default;
    GStreamerRTCStreamVideoEncoder(const GStreamerRTCStreamVideoEncoder &other)                 = delete;
    GStreamerRTCStreamVideoEncoder &operator=(const GStreamerRTCStreamVideoEncoder &other)      = delete;
    GStreamerRTCStreamVideoEncoder(GStreamerRTCStreamVideoEncoder &&other) noexcept             = default;
    GStreamerRTCStreamVideoEncoder &operator=(GStreamerRTCStreamVideoEncoder &&other) noexcept  = default;
    virtual ~GStreamerRTCStreamVideoEncoder()                                                   = default;
};

#else

using GStreamerRTCStreamVideoEncoder = NullRTCStreamVideoEncoder;

#endif


enum RTCStreamType
{
    RTC_STREAM_TYPE_UNKNOWN = 0,
    RTC_STREAM_TYPE_AUDIO,
    RTC_STREAM_TYPE_VIDEO
};

class RTCStream
{
public:
    RTCStream(RTCStreamType stream_type)
        : m_stream_type(stream_type)
    {
    }

    RTCStream(const RTCStream &other)                   = delete;
    RTCStream &operator=(const RTCStream &other)        = delete;
    RTCStream(RTCStream &&other) noexcept               = default;
    RTCStream &operator=(RTCStream &&other) noexcept    = default;
    virtual ~RTCStream()                                = default;

    RTCStreamType GetStreamType() const
        { return m_stream_type; }

protected:
    RTCStreamType m_stream_type;
};

class NullRTCStream : public RTCStream
{
public:
    NullRTCStream(RTCStreamType stream_type)
        : RTCStream(stream_type)
    {
    }

    NullRTCStream(const NullRTCStream &other)                   = delete;
    NullRTCStream &operator=(const NullRTCStream &other)        = delete;
    NullRTCStream(NullRTCStream &&other) noexcept               = default;
    NullRTCStream &operator=(NullRTCStream &&other) noexcept    = default;
    virtual ~NullRTCStream() override                           = default;
};

#ifdef HYP_LIBDATACHANNEL

class LibDataChannelRTCStream : public RTCStream
{
public:
    LibDataChannelRTCStream(RTCStreamType stream_type)
        : RTCStream(stream_type)
    {
    }

    LibDataChannelRTCStream(const LibDataChannelRTCStream &other)                   = delete;
    LibDataChannelRTCStream &operator=(const LibDataChannelRTCStream &other)        = delete;
    LibDataChannelRTCStream(LibDataChannelRTCStream &&other) noexcept               = default;
    LibDataChannelRTCStream &operator=(LibDataChannelRTCStream &&other) noexcept    = default;
    virtual ~LibDataChannelRTCStream() override                                     = default;
};

#else

using LibDataChannelRTCStream = NullRTCStream;

#endif // HYP_LIBDATACHANNEL

} // namespace hyperion::v2

#endif