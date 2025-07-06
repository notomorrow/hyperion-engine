/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/utilities/Optional.hpp>

#include <core/functional/Proc.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/object/HypObject.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/Defines.hpp>

namespace hyperion {

namespace filesystem {
class DataStore;
} // namespace filesystem

using filesystem::DataStore;

HYP_DECLARE_LOG_CHANNEL(Streaming);

class StreamedDataBase;
class StreamingThread;

enum class StreamedDataState : uint32
{
    NONE = 0,
    LOADED,
    UNPAGED
};

class HYP_API StreamedDataBase : public EnableRefCountedPtrFromThis<StreamedDataBase>, public ResourceBase
{
    using UnpagingSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;

protected:
    /*! \brief Construct the StreamedDataBase with the given initial state. If the state is LOADED, \ref{outResourceHandle} will be set to a resource handle for this. */
    StreamedDataBase(StreamedDataState initialState, ResourceHandle& outResourceHandle);

public:
    StreamedDataBase() = default;
    StreamedDataBase(const StreamedDataBase&) = delete;
    StreamedDataBase& operator=(const StreamedDataBase&) = delete;
    StreamedDataBase(StreamedDataBase&&) noexcept = delete;
    StreamedDataBase& operator=(StreamedDataBase&&) noexcept = delete;
    virtual ~StreamedDataBase() = default;

    void Unpage();
    void Load() const;

    virtual HashCode GetDataHashCode() const = 0;

    virtual const ByteBuffer& GetByteBuffer() const;

protected:
    virtual void Initialize() override final;
    virtual void Destroy() override final;
    virtual void Update() override final;

    virtual ThreadBase* GetOwnerThread() const override final;

    mutable Mutex m_mutex;

private:
    virtual bool IsInMemory_Internal() const = 0;

    virtual void Load_Internal() const = 0;
    virtual void Unpage_Internal() = 0;
};

class HYP_API NullStreamedData final : public StreamedDataBase
{
public:
    NullStreamedData() = default;
    virtual ~NullStreamedData() override = default;

protected:
    virtual HashCode GetDataHashCode() const override
    {
        return HashCode(0);
    }

private:
    virtual bool IsInMemory_Internal() const override;

    virtual void Load_Internal() const override;
    virtual void Unpage_Internal() override;
};

class HYP_API MemoryStreamedData final : public StreamedDataBase
{
public:
    MemoryStreamedData(HashCode hashCode, Proc<bool(HashCode, ByteBuffer&)>&& loadFromMemoryProc = {});
    MemoryStreamedData(HashCode hashCode, const ByteBuffer& byteBuffer, StreamedDataState initialState, ResourceHandle& outResourceHandle);
    MemoryStreamedData(HashCode hashCode, ByteBuffer&& byteBuffer, StreamedDataState initialState, ResourceHandle& outResourceHandle);
    MemoryStreamedData(HashCode hashCode, ConstByteView byteView, StreamedDataState initialState, ResourceHandle& outResourceHandle);

    MemoryStreamedData(const MemoryStreamedData& other) = delete;
    MemoryStreamedData& operator=(const MemoryStreamedData& other) = delete;

    MemoryStreamedData(MemoryStreamedData&& other) noexcept = delete;
    MemoryStreamedData& operator=(MemoryStreamedData&& other) noexcept = delete;

    virtual ~MemoryStreamedData() override = default;

protected:
    virtual const ByteBuffer& GetByteBuffer() const override;

    virtual HashCode GetDataHashCode() const override
    {
        return m_hashCode;
    }

private:
    virtual bool IsInMemory_Internal() const override;

    virtual void Load_Internal() const override;
    virtual void Unpage_Internal() override;

    HashCode m_hashCode;
    mutable Optional<ByteBuffer> m_byteBuffer;
    Proc<bool(HashCode, ByteBuffer&)> m_loadFromMemoryProc;

    DataStore* m_dataStore;
    ResourceHandle m_dataStoreResourceHandle;
};

} // namespace hyperion
