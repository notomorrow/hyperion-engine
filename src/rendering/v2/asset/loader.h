#ifndef HYPERION_V2_LOADER_H
#define HYPERION_V2_LOADER_H

#include <asset/byte_reader.h>
#include <asset/buffered_text_reader.h>

#include <vector>

#define HYP_V2_LOADER_BUFFER_SIZE 2048

namespace hyperion::v2 {

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

class LoaderBase {};

template <class FinalType, class T = FinalType>
class Loader : public LoaderBase {
public:
    using Object  = LoaderObject<T>;
    using Results = std::vector<std::pair<LoaderResult, Object>>;
    
    struct Handler {
        std::function<LoaderResult(LoaderStream *, LoaderObject<T> &)>     load_fn;
        std::function<std::unique_ptr<FinalType>(const LoaderObject<T> &)> build_fn;
    };

private:
    class Impl {
    public:
        Impl(const Handler &handler)
            : m_handler(handler) {}

        Impl &Enqueue(const LoaderResourceKey &key, LoaderResource &&resource)
        {
            m_resources.push_back(std::move(resource));

            return *this;
        }
        
        Results Load()
        {
            Results results;
            results.reserve(m_resources.size());

            for (auto &resource : m_resources) {
                if (resource.stream == nullptr) {
                    results.push_back(std::make_pair(
                        LoaderResult{LoaderResult::Status::ERR, "No byte stream provided"},
                        Object{}
                    ));

                    continue;
                }

                if (!resource.stream->IsOpen()) {
                    results.push_back(std::make_pair(
                        LoaderResult{LoaderResult::Status::ERR, "Failed to open file"},
                        Object{}
                    ));

                    continue;
                }

                if (resource.stream->Eof()) {
                    results.push_back(std::make_pair(
                        LoaderResult{LoaderResult::Status::ERR, "Byte stream in EOF state"},
                        Object{}
                    ));

                    continue;
                }

                Object object;
                LoaderResult result = m_handler.load_fn(resource.stream.get(), object);
            
                results.push_back(std::make_pair(result, std::move(object)));
            }

            m_resources.clear();

            return results;
        }

    private:
        Handler m_handler;

        std::vector<LoaderResource> m_resources;
    };

public:

    Loader(Handler &&handler)
        : m_handler(std::move(handler))
    {
    }

    Loader(const Loader &other) = delete;
    Loader &operator=(const Loader &other) = delete;

    Loader(Loader &&other) noexcept
        : m_handler(std::move(other.m_handler))
    {
    }

    Loader &operator=(Loader &&other) noexcept
    {
        m_handler = std::move(other.m_handler);

        return *this;
    }

    ~Loader() = default;

    Impl Instance() const
        { return Impl(m_handler); }

    std::unique_ptr<FinalType> Build(const Object &object) const
    {
        AssertThrowMsg(m_handler.build_fn, "No object build function set");

        return std::move(m_handler.build_fn(object));
    }

private:
    Handler m_handler;
};

} // namespace hyperion::v2

#endif