/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <util/profiling/ProfileScope.hpp>
#include <util/profiling/PerformanceClock.hpp>

#include <core/containers/LinkedList.hpp>
#include <core/containers/FlatMap.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Thread.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/net/HTTPRequest.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/system/AppContext.hpp>
#include <core/system/CommandLine.hpp>

#include <util/json/JSON.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region ProfilerConnectionThread

class ProfilerConnectionThread final : public Thread<Scheduler, ProfilerConnection *>
{
public:
    ProfilerConnectionThread()
        : Thread(Name::Unique("ProfilerConnectionThread"), ThreadPriorityValue::LOWEST),
          m_is_running { false },
          m_stop_requested { false }
    {
    }

    /*! \brief Atomically load the boolean value indicating that this thread is actively running */
    bool IsRunning() const
        { return m_is_running.Get(MemoryOrder::RELAXED); }

    void Stop()
        { m_stop_requested.Set(true, MemoryOrder::RELAXED); }

private:
    virtual void operator()(ProfilerConnection *profiler_connection) override
    {
        m_is_running.Set(true, MemoryOrder::RELAXED);

        if (StartConnection(profiler_connection)) {
            while (!m_stop_requested.Get(MemoryOrder::RELAXED)) {
                DoWork(profiler_connection);
            }
        }

        m_is_running.Set(false, MemoryOrder::RELAXED);
    }

    bool StartConnection(ProfilerConnection *profiler_connection);
    void DoWork(ProfilerConnection *profiler_connection);

    AtomicVar<bool> m_is_running;
    AtomicVar<bool> m_stop_requested;
};

#pragma endregion ProfilerConnectionThread

#pragma region ProfilerConnection

class ProfilerConnection
{
public:
    static ProfilerConnection &GetInstance()
    {
        static ProfilerConnection instance;

        return instance;
    }

    ProfilerConnection()
    {
        json::JSONValue trace_url = g_engine->GetAppContext()->GetArguments()["TraceURL"];
        
        if (trace_url.IsString()) {
            m_endpoint_url = trace_url.AsString();
        }
    }

    ProfilerConnection(const ProfilerConnection &other)                 = delete;
    ProfilerConnection &operator=(const ProfilerConnection &other)      = delete;
    ProfilerConnection(ProfilerConnection &&other) noexcept             = delete;
    ProfilerConnection &operator=(ProfilerConnection &&other) noexcept  = delete;
    
    ~ProfilerConnection()
    {   
        StopThread();

        for (Task<net::HTTPResponse> &task : m_requests) {
            task.Await();
        }
    }

    const String &GetEndpointURL() const
    {
        return m_endpoint_url;
    }

    void SetEndpointURL(const String &endpoint_url)
    {
        // @TODO: ensure thread not running - restart it on change
        m_endpoint_url = endpoint_url;
    }

    void StartThread()
    {
        if (m_thread.IsRunning()) {
            return;
        }

        m_thread.Start(this);
    }

    void StopThread()
    {
        if (m_thread.IsRunning()) {
            m_thread.Stop();
        }

        if (m_thread.CanJoin()) {
            m_thread.Join();
        }
    }

    void IterateRequests()
    {
        HYP_LOG(Profile, Info, "Iterate requests ({})", m_requests.Size());

        Threads::AssertOnThread(m_thread.GetID());

        // iterate over completed requests
        for (auto it = m_requests.Begin(); it != m_requests.End();) {
            if (it->IsCompleted()) {
                it = m_requests.Erase(it);
                continue;
            }

            ++it;
        }
    }

    void Push(Array<json::JSONValue> &&values)
    {
        const ThreadID current_thread_id = Threads::CurrentThreadID();

        Array<json::JSONValue> *json_values_array = nullptr;

        { // critical section - may invalidate iterators
            Mutex::Guard guard(m_values_mutex);

            auto it = m_per_thread_values.Find(current_thread_id);

            if (it == m_per_thread_values.End()) {
                it = m_per_thread_values.Insert(current_thread_id, MakeUnique<Array<json::JSONValue>>()).first;
            }

            json_values_array = it->second.Get();
        }

        // unique per thread; fine outside of mutex
        json_values_array->Concat(std::move(values));
    }

    bool StartConnection()
    {
        Threads::AssertOnThread(m_thread.GetID());

        if (m_endpoint_url.Empty()) {
            HYP_LOG(Profile, Error, "Profiler connection endpoint URL not set, cannot start connection.");

            return false;
        }

        m_trace_id = UUID();

        json::JSONObject object;
        object["trace_id"] = m_trace_id.ToString();

        Task<net::HTTPResponse> start_request = net::HTTPRequest(m_endpoint_url + "/start", json::JSONValue(std::move(object)), net::HTTPMethod::POST)
            .Send();

        HYP_LOG(Profile, Info, "Waiting for profiler connection request to finish");

        net::HTTPResponse &response = start_request.Await();

        if (!response.IsSuccess()) {
            HYP_LOG(Profile, Error, "Failed to connect to profiler connection endpoint! Status code: {}", response.GetStatusCode());

            return false;
        }

        return true;
    }

    void Submit()
    {
        Threads::AssertOnThread(m_thread.GetID());

        if (m_endpoint_url.Empty()) {
            HYP_LOG(Profile, Warning, "Profiler connection endpoint URL not set, cannot submit results.");

            return;
        }

        HYP_LOG(Profile, Info, "Submitting profiler results to trace server...");

        json::JSONObject object;

        { // critical section
            Mutex::Guard guard(m_values_mutex);

            json::JSONArray groups_array;

            for (KeyValuePair<ThreadID, UniquePtr<json::JSONArray>> &it : m_per_thread_values) {
                json::JSONObject group_object;
                group_object["name"] = json::JSONString(it.first.name.LookupString());
                group_object["values"] = std::move(*it.second); // move it so it clears current values
                groups_array.PushBack(std::move(group_object));
            }

            object["groups"] = std::move(groups_array);
        }

        // Send request with all queued data
        net::HTTPRequest request(m_endpoint_url + "/results", json::JSONValue(std::move(object)), net::HTTPMethod::POST);
        m_requests.PushBack(request.Send());
    }

private:
    String                                          m_endpoint_url;
    UUID                                            m_trace_id;
    ProfilerConnectionThread                        m_thread;

    FlatMap<ThreadID, UniquePtr<json::JSONArray>>   m_per_thread_values;
    mutable Mutex                                   m_values_mutex;

    Array<Task<net::HTTPResponse>>                  m_requests;


};

HYP_API void SetProfilerConnectionEndpoint(ANSIStringView endpoint_url)
{
    ProfilerConnection::GetInstance().SetEndpointURL(endpoint_url);
}

HYP_API void StartProfilerConnectionThread()
{
#ifdef HYP_ENABLE_PROFILE
    ProfilerConnection::GetInstance().StartThread();
#endif
}

HYP_API void StopProfilerConnectionThread()
{
#ifdef HYP_ENABLE_PROFILE
    ProfilerConnection::GetInstance().StopThread();
#endif
}

#pragma endregion ProfilerConnection

bool ProfilerConnectionThread::StartConnection(ProfilerConnection *profiler_connection)
{
    return profiler_connection->StartConnection();
}

void ProfilerConnectionThread::DoWork(ProfilerConnection *profiler_connection)
{
    profiler_connection->IterateRequests();

    Threads::Sleep(100);

    profiler_connection->Submit();
}

#pragma region ProfileScopeEntry

struct ProfileScopeEntry
{
    const ANSIString                label;
    const ANSIStringView            location;
    uint64                          start_timestamp_us;
    uint64                          measured_time_us;

    ProfileScopeEntry               *parent = nullptr;
    LinkedList<ProfileScopeEntry>   children;

    ProfileScopeEntry(ANSIStringView label, ANSIStringView location, ProfileScopeEntry *parent = nullptr)
        : label(label),
          location(location),
          start_timestamp_us(0),
          measured_time_us(0),
          parent(parent)
    {
        StartMeasure();
    }

    ProfileScopeEntry(const ProfileScopeEntry &other) = delete;
    ProfileScopeEntry &operator=(const ProfileScopeEntry &other) = delete;

    HYP_FORCE_INLINE void StartMeasure()
    {
        start_timestamp_us = PerformanceClock::Now();
        measured_time_us = 0;
    }

    HYP_FORCE_INLINE void SaveDiff()
    {
        measured_time_us = PerformanceClock::TimeSince(start_timestamp_us);
    }

    json::JSONValue ToJSON(ProfileScopeEntry *parent_scope = nullptr) const
    {
        json::JSONObject object;
        object["label"] = json::JSONString(label);
        object["location"] = json::JSONString(location);
        object["start_timestamp_ms"] = json::JSONNumber(start_timestamp_us / 1000);
        object["measured_time_us"] = json::JSONNumber(measured_time_us);
        
        json::JSONArray children_array;

        for (const ProfileScopeEntry &child : children) {
            children_array.PushBack(child.ToJSON());
        }

        object["children"] = std::move(children_array);

        return json::JSONValue(std::move(object));
    }
};

#pragma endregion ProfileScopeEntry

#pragma region ProfileScopeEntryQueue

struct ProfileScopeEntryQueue
{
    Time                        start_time;
    Array<ProfileScopeEntry>    entries;

    json::JSONValue ToJSON() const
    {
        json::JSONArray array;

        for (const ProfileScopeEntry &entry : entries) {
            array.PushBack(entry.ToJSON());
        }

        json::JSONObject object;
        object["start_time"] = uint64(start_time);
        object["entries"] = std::move(array);
        
        return json::JSONValue(std::move(object));
    }
};

#pragma endregion ProfileScopeEntryQueue

#pragma region ProfileScopeStack

static void DebugLogProfileScopeEntry(ProfileScopeEntry *entry, int depth = 0)
{
    if (depth > 0) {
        for (int i = 0; i < depth; i++) {
            putchar(int(' '));
        }

        DebugLog(LogType::Debug, "Profile scope entry '%s': %llu us\n", entry->label.Data(), entry->measured_time_us);
    }

    for (ProfileScopeEntry &child : entry->children) {
        DebugLogProfileScopeEntry(&child, depth + 1);
    }
}

class ProfileScopeStack
{
public:
    ProfileScopeStack()
        : m_thread_id(Threads::CurrentThreadID()),
          m_root_entry("ROOT", ""),
          m_head(&m_root_entry)
    {
        m_root_entry.StartMeasure();
    }

    ~ProfileScopeStack() = default;

    void Reset()
    {
        Threads::AssertOnThread(m_thread_id);

        m_root_entry.SaveDiff();

        if (g_engine->GetAppContext()->GetArguments()["Profile"]) {
            m_queue.PushBack(m_root_entry.ToJSON());
            
            if (m_queue.Size() >= 100) {
                // DebugLogProfileScopeEntry(&m_root_entry);

                ProfilerConnection::GetInstance().Push(std::move(m_queue));
            }
        }

        m_root_entry.children.Clear();
        m_root_entry.StartMeasure();

        m_head = &m_root_entry;
    }

    ProfileScopeEntry &Open(ANSIStringView label, ANSIStringView location)
    {
        Threads::AssertOnThread(m_thread_id);
        
        m_head = &m_head->children.EmplaceBack(label, location, m_head);
        return *m_head;
    }

    void Close()
    {
        Threads::AssertOnThread(m_thread_id);

        m_head->SaveDiff();

        AssertThrow(m_head->parent != nullptr);

        m_head = m_head->parent;
    }

private:
    ThreadID                        m_thread_id;
    ProfileScopeEntry               m_root_entry;
    ProfileScopeEntry               *m_head;

    json::JSONArray                 m_queue;
};

#pragma endregion ProfileScopeStack

#pragma region ProfileScope

ProfileScopeStack &ProfileScope::GetProfileScopeStackForCurrentThread()
{
    static thread_local ProfileScopeStack profile_scope_stack;

    return profile_scope_stack;
}

void ProfileScope::ResetForCurrentThread()
{
    GetProfileScopeStackForCurrentThread().Reset();
}

ProfileScope::ProfileScope(ANSIStringView label, ANSIStringView location)
    : entry(&GetProfileScopeStackForCurrentThread().Open(label, location))
{
}

ProfileScope::~ProfileScope()
{
    GetProfileScopeStackForCurrentThread().Close();
}

#pragma endregion ProfileScope

} // namespace hyperion