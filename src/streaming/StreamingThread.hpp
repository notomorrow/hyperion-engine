/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/TaskThread.hpp>

#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

class StreamingThread final : public TaskThread
{
public:
    StreamingThread();
    virtual ~StreamingThread() override;
};

HYP_API void SetGlobalStreamingThread(const RC<StreamingThread>& streamingThread);
HYP_API const RC<StreamingThread>& GetGlobalStreamingThread();

} // namespace hyperion
