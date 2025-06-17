/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedData.hpp>
#include <streaming/StreamingThread.hpp>

#include <core/filesystem/DataStore.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/containers/StaticString.hpp>

#include <core/object/HypClass.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region StreamedDataBase

StreamedDataBase::StreamedDataBase(StreamedDataState initialState, ResourceHandle& outResourceHandle)
{
    switch (initialState)
    {
    case StreamedDataState::LOADED:
        // Setup resource handle so the data stays loaded
        // Initialize can't be called since it is a virtual function and the object is not fully constructed yet
        outResourceHandle = ResourceHandle(*this, /* shouldInitialize */ false);

        break;
    default:
        // Do nothing
        break;
    }
}

void StreamedDataBase::Initialize()
{
    Load_Internal();
}

void StreamedDataBase::Destroy()
{
    HYP_NAMED_SCOPE_FMT("Unpage {} (StreamedDataBase::Destroy())", InstanceClass()->GetName());

    Unpage_Internal();
}

void StreamedDataBase::Update()
{
}

void StreamedDataBase::Load() const
{
    const_cast<StreamedDataBase*>(this)->Execute([this]()
        {
            if (IsInMemory_Internal())
            {
                return;
            }

            HYP_NAMED_SCOPE("Load {}", InstanceClass()->GetName());

            Load_Internal();
        });
}

void StreamedDataBase::Unpage()
{
    Execute([this]()
        {
            HYP_NAMED_SCOPE("Unpage {} (manual)", InstanceClass()->GetName());

            Unpage_Internal();
        });
}

const ByteBuffer& StreamedDataBase::GetByteBuffer() const
{
    static const ByteBuffer defaultValue {};

    return defaultValue;
}

ThreadBase* StreamedDataBase::GetOwnerThread() const
{
    return GetGlobalStreamingThread().Get();
}

#pragma endregion StreamedDataBase

#pragma region NullStreamedData

bool NullStreamedData::IsInMemory_Internal() const
{
    return false;
}

void NullStreamedData::Unpage_Internal()
{
    // Do nothing
}

void NullStreamedData::Load_Internal() const
{
    // Do nothing
}

#pragma endregion NullStreamedData

#pragma region MemoryStreamedData

MemoryStreamedData::MemoryStreamedData(HashCode hashCode, Proc<bool(HashCode, ByteBuffer&)>&& loadFromMemoryProc)
    : StreamedDataBase(),
      m_hashCode(hashCode),
      m_loadFromMemoryProc(std::move(loadFromMemoryProc)),
      m_dataStore(&GetDataStore<HYP_STATIC_STRING("streaming"), DSF_RW>()),
      m_dataStoreResourceHandle(*m_dataStore)
{
    const bool shouldLoadUnpaged = !m_dataStore->Exists(String::ToString(hashCode.Value())) && m_loadFromMemoryProc.IsValid();

    if (shouldLoadUnpaged)
    {
        MemoryStreamedData::Load_Internal();
        MemoryStreamedData::Unpage_Internal();
    }
}

MemoryStreamedData::MemoryStreamedData(HashCode hashCode, const ByteBuffer& byteBuffer, StreamedDataState initialState, ResourceHandle& outResourceHandle)
    : StreamedDataBase(initialState, outResourceHandle),
      m_hashCode(hashCode)
{
    if (initialState == StreamedDataState::LOADED)
    {
        m_byteBuffer.Set(byteBuffer);
    }
}

MemoryStreamedData::MemoryStreamedData(HashCode hashCode, ByteBuffer&& byteBuffer, StreamedDataState initialState, ResourceHandle& outResourceHandle)
    : StreamedDataBase(initialState, outResourceHandle),
      m_hashCode(hashCode)
{
    if (initialState == StreamedDataState::LOADED)
    {
        m_byteBuffer.Set(std::move(byteBuffer));
    }
}

MemoryStreamedData::MemoryStreamedData(HashCode hashCode, ConstByteView byteView, StreamedDataState initialState, ResourceHandle& outResourceHandle)
    : StreamedDataBase(initialState, outResourceHandle),
      m_hashCode(hashCode)
{
    if (initialState == StreamedDataState::LOADED)
    {
        m_byteBuffer.Set(ByteBuffer(byteView));
    }
}

bool MemoryStreamedData::IsInMemory_Internal() const
{
    return m_byteBuffer.HasValue();
}

void MemoryStreamedData::Unpage_Internal()
{
    if (!m_byteBuffer.HasValue())
    {
        return;
    }

    ByteBuffer byteBuffer = std::move(*m_byteBuffer);
    m_byteBuffer.Unset();

    if (byteBuffer.Empty())
    {
        return;
    }

    m_dataStore->Write(String::ToString(m_hashCode.Value()), byteBuffer);
}

void MemoryStreamedData::Load_Internal() const
{
    if (!m_byteBuffer.HasValue())
    {
        m_byteBuffer.Emplace();

        if (!m_dataStore->Read(String::ToString(m_hashCode.Value()), *m_byteBuffer))
        {
            if (m_loadFromMemoryProc.IsValid())
            {
                if (m_loadFromMemoryProc(m_hashCode, *m_byteBuffer))
                {
                    return;
                }

                HYP_LOG(Streaming, Warning, "Failed to load streamed data with hash code {} from memory", m_hashCode.Value());
            }

            m_byteBuffer.Unset();
        }
    }
}

const ByteBuffer& MemoryStreamedData::GetByteBuffer() const
{
    if (m_byteBuffer.HasValue())
    {
        return *m_byteBuffer;
    }

    return StreamedDataBase::GetByteBuffer();
}

#pragma endregion MemoryStreamedData

} // namespace hyperion