/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamingThread.hpp>

#include <core/threading/Mutex.hpp>

namespace hyperion {

#pragma region StreamingThread

static RC<StreamingThread> g_globalStreamingThread;
static Mutex g_globalStreamingThreadMutex;

HYP_API void SetGlobalStreamingThread(const RC<StreamingThread>& streamingThread)
{
    Mutex::Guard guard(g_globalStreamingThreadMutex);

    g_globalStreamingThread = streamingThread;
}

HYP_API const RC<StreamingThread>& GetGlobalStreamingThread()
{
    Mutex::Guard guard(g_globalStreamingThreadMutex);

    return g_globalStreamingThread;
}

StreamingThread::StreamingThread()
    : TaskThread(NAME("StreamingThread"), ThreadPriorityValue::LOW)
{
}

StreamingThread::~StreamingThread() = default;

#pragma endregion StreamingThread

} // namespace hyperion