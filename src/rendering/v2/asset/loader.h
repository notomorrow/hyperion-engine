#ifndef HYPERION_V2_LOADER_H
#define HYPERION_V2_LOADER_H

#include <asset/byte_reader.h>
#include <asset/buffered_text_reader.h>

#include <vector>

#define HYP_V2_LOADER_BUFFER_SIZE 2048
#define HYP_V2_LOADER_THREADED 1

#if HYP_V2_LOADER_THREADED
#include <thread>
#include <mutex>
#endif

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

    LoaderImpl &Enqueue(const LoaderResourceKey &key, LoaderResource &&resource)
    {
        m_resources.push_back(std::move(resource));

        return *this;
    }
    
    Results Load()
    {
        Results results;
        results.resize(m_resources.size());

        std::vector<std::thread> threads;
        threads.resize(m_resources.size());

        for (size_t i = 0; i < m_resources.size(); i++) {
            threads[i] = std::thread([index = i, &resources = m_resources, &handler = m_handler, &results] {
                const auto &resource = resources[index];

                if (resource.stream == nullptr) {
                    results[index] = std::make_pair(
                        LoaderResult{LoaderResult::Status::ERR, "No byte stream provided"},
                        Object{}
                    );

                    return;
                }

                if (!resource.stream->IsOpen()) {
                    results[index] = std::make_pair(
                        LoaderResult{LoaderResult::Status::ERR, "Failed to open file"},
                        Object{}
                    );

                    return;
                }

                if (resource.stream->Eof()) {
                    results[index] = std::make_pair(
                        LoaderResult{LoaderResult::Status::ERR, "Byte stream in EOF state"},
                        Object{}
                    );

                    return;
                }

                Object object;
                LoaderResult result = handler.load_fn(resource.stream.get(), object);
            
                results[index] = std::make_pair(result, std::move(object));
            });
        }

        for (auto &thread : threads) {
            thread.join();
        }

        m_resources.clear();

        return results;
    }

private:
    Handler m_handler;

    std::vector<LoaderResource> m_resources;
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