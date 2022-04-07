#ifndef HYPERION_V2_LOADER_H
#define HYPERION_V2_LOADER_H

#include <asset/byte_reader.h>
#include <asset/buffered_text_reader.h>

#include <vector>

#define HYP_V2_LOADER_BUFFER_SIZE 2048

namespace hyperion::v2 {

class Engine;

using LoaderStream = BufferedTextReader<HYP_V2_LOADER_BUFFER_SIZE>;

struct LoaderResult {
    enum class Status {
        OK,
        ERR
    };

    Status status = Status::OK;
    std::string message;

    operator bool() const
        { return status == Status::OK; }
};

using LoaderResourceKey = std::string;

struct LoaderResource {
    std::unique_ptr<LoaderStream> stream;
    LoaderResult result;
};

template <class Object, class Handler>
class LoaderImpl {
    using Results = std::vector<std::pair<LoaderResult, Object>>;

public:
    LoaderImpl(const Handler &handler)
        : m_handler(handler) {}

    std::pair<LoaderResult, Object> Load(LoaderResource &&resource)
    {
        if (resource.stream == nullptr) {
            return std::make_pair(
                LoaderResult{ LoaderResult::Status::ERR, "No byte stream provided" },
                Object{}
            );
        }

        if (!resource.stream->IsOpen()) {
            return std::make_pair(
                LoaderResult{ LoaderResult::Status::ERR, "Failed to open file" },
                Object{}
            );
        }

        if (resource.stream->Eof()) {
            return std::make_pair(
                LoaderResult{ LoaderResult::Status::ERR, "Byte stream in EOF state" },
                Object{}
            );
        }

        Object object;
        LoaderResult result = m_handler.load_fn(resource.stream.get(), object);

        return std::make_pair(result, std::move(object));
    }

private:
    Handler m_handler;
};

template <class T, LoaderFormat Format>
class LoaderBase {
public:
    using FinalType = T;
    using Object  = LoaderObject<T, Format>;
    
    struct Handler {
        std::function<LoaderResult(LoaderStream *, Object &)>     load_fn;
        std::function<std::unique_ptr<FinalType>(Engine *engine, const Object &)> build_fn;
    };

public:

    LoaderBase(Handler &&handler)
        : m_handler(std::move(handler))
    {
    }

    LoaderBase(const LoaderBase &other) = delete;
    LoaderBase &operator=(const LoaderBase &other) = delete;

    LoaderBase(LoaderBase &&other) noexcept
        : m_handler(std::move(other.m_handler))
    {
    }

    LoaderBase &operator=(LoaderBase &&other) noexcept
    {
        m_handler = std::move(other.m_handler);

        return *this;
    }

    ~LoaderBase() = default;

    LoaderImpl<Object, Handler> Instance() const
        { return LoaderImpl<Object, Handler>(m_handler); }

    std::unique_ptr<FinalType> Build(Engine *engine, const Object &object) const
    {
        AssertThrowMsg(m_handler.build_fn, "No object build function set");

        return std::move(m_handler.build_fn(engine, object));
    }

private:
    Handler m_handler;
};

} // namespace hyperion::v2

#endif