/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedTextureData.hpp>

#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/io/ByteWriter.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/object/HypData.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

StreamedTextureData::StreamedTextureData(StreamedDataState initialState, TextureData textureData, ResourceHandle& outResourceHandle)
    : StreamedDataBase(initialState, outResourceHandle),
      m_textureDesc(textureData.desc),
      m_bufferSize(textureData.buffer.Size())
{
    switch (initialState)
    {
    case StreamedDataState::NONE:
        m_streamedData.EmplaceAs<NullStreamedData>();

        break;
    case StreamedDataState::LOADED: // fallthrough
    case StreamedDataState::UNPAGED:
        m_textureData.Set(std::move(textureData));

        m_streamedData.EmplaceAs<MemoryStreamedData>(m_textureData->GetHashCode(), [this](HashCode hc, ByteBuffer& out) -> bool
            {
                if (!m_textureData)
                {
                    HYP_LOG(Streaming, Error, "Texture data is unset when trying to load from memory");

                    return false;
                }

                MemoryByteWriter writer;

                FBOMWriter serializer { FBOMWriterConfig {} };

                if (FBOMResult err = serializer.Append(*m_textureData))
                {
                    HYP_LOG(Streaming, Error, "Failed to write streamed data: {}", err.message);

                    return false;
                }

                if (FBOMResult err = serializer.Emit(&writer))
                {
                    HYP_LOG(Streaming, Error, "Failed to write streamed data: {}", err.message);

                    return false;
                }

                out = std::move(writer.GetBuffer());

                return true;
            });

        break;
    default:
        HYP_NOT_IMPLEMENTED_VOID();
    }
}

StreamedTextureData::StreamedTextureData()
    : StreamedDataBase(),
      m_streamedData(MakeRefCountedPtr<NullStreamedData>())
{
}

StreamedTextureData::StreamedTextureData(const TextureData& textureData, ResourceHandle& outResourceHandle)
    : StreamedTextureData(textureData.buffer.Any() ? StreamedDataState::LOADED : StreamedDataState::NONE, textureData, outResourceHandle)
{
}

StreamedTextureData::StreamedTextureData(TextureData&& textureData, ResourceHandle& outResourceHandle)
    : StreamedTextureData(textureData.buffer.Any() ? StreamedDataState::LOADED : StreamedDataState::NONE, std::move(textureData), outResourceHandle)
{
}

bool StreamedTextureData::IsInMemory_Internal() const
{
    return m_textureData.HasValue();
}

void StreamedTextureData::Unpage_Internal()
{
    if (m_streamedData)
    {
        m_streamedData->Unpage();
    }

    m_textureData.Unset();
}

void StreamedTextureData::Load_Internal() const
{
    AssertThrow(m_streamedData != nullptr);
    m_streamedData->Load();

    if (!m_textureData.HasValue())
    {
        LoadTextureData(m_streamedData->GetByteBuffer());
    }
}

void StreamedTextureData::LoadTextureData(const ByteBuffer& byteBuffer) const
{
    m_textureData.Unset();

    MemoryBufferedReaderSource source { byteBuffer.ToByteView() };
    BufferedReader reader { &source };

    if (!reader.IsOpen())
    {
        return;
    }

    HypData value;

    FBOMReader deserializer { FBOMReaderConfig {} };
    FBOMLoadContext context;

    if (FBOMResult err = deserializer.Deserialize(context, reader, value))
    {
        HYP_LOG(Streaming, Warning, "StreamedTextureData: Error deserializing texture data for StreamedTextureData with hash: {}\tError: {}", GetDataHashCode().Value(), err.message);
        return;
    }

    TextureData& textureData = value.Get<TextureData>();

    if (textureData.buffer.Empty())
    {
        HYP_LOG(Streaming, Warning, "StreamedTextureData: Texture data buffer is empty for StreamedTextureData with hash: {}", GetDataHashCode().Value());

        return;
    }

    m_textureData = textureData;
    AssertThrow(m_textureDesc == m_textureData->desc);

    m_bufferSize = m_textureData->buffer.Size();

    AssertThrowMsg(m_bufferSize == m_textureDesc.GetByteSize(),
        "Buffer size mismatch for StreamedTextureData with hash: %llu. Expected: %u, Actual: %u",
        GetDataHashCode().Value(), m_textureDesc.GetByteSize(), m_bufferSize);
}

const TextureData& StreamedTextureData::GetTextureData() const
{
    WaitForTaskCompletion();

    if (!m_textureData.HasValue())
    {
        static const TextureData defaultValue {};

        return defaultValue;
    }

    return m_textureData.Get();
}

void StreamedTextureData::SetTextureData(TextureData&& textureData)
{
    Execute([this, textureData = std::move(textureData)]()
        {
            m_textureDesc = textureData.desc;
            m_bufferSize = textureData.buffer.Size();

            m_textureData.Set(std::move(textureData));
        });
}

const TextureDesc& StreamedTextureData::GetTextureDesc() const
{
    WaitForTaskCompletion();

    if (!m_textureData.HasValue())
    {
        static const TextureDesc defaultValue {};

        return defaultValue;
    }

    return m_textureData->desc;
}

void StreamedTextureData::SetTextureDesc(const TextureDesc& textureDesc)
{
    Execute([this, textureDesc]()
        {
            m_textureDesc = textureDesc;

            if (m_textureData.HasValue())
            {
                m_textureData->desc = textureDesc;
            }
        });
}

} // namespace hyperion