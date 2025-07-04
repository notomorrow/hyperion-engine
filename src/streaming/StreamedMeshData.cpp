/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedMeshData.hpp>

#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/io/ByteWriter.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

StreamedMeshData::StreamedMeshData(StreamedDataState initialState, MeshData&& meshData, ResourceHandle& outResourceHandle)
    : StreamedDataBase(initialState, outResourceHandle),
      m_streamedData(nullptr),
      m_numVertices(meshData.vertices.Size()),
      m_numIndices(meshData.indices.Size())
{
    switch (initialState)
    {
    case StreamedDataState::NONE:
        m_streamedData = MakeRefCountedPtr<NullStreamedData>();

        break;
    case StreamedDataState::LOADED: // fallthrough
    case StreamedDataState::UNPAGED:
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        m_meshData.Set(std::move(meshData));

        m_streamedData = MakeRefCountedPtr<MemoryStreamedData>(m_meshData->GetHashCode(), [this](HashCode hc, ByteBuffer& out) -> bool
            {
                HYP_MT_CHECK_READ(m_dataRaceDetector);

                if (!m_meshData.HasValue())
                {
                    HYP_LOG(Streaming, Error, "StreamedMeshData: Mesh data is not set when attempting to load from memory!");

                    return false;
                }

                MemoryByteWriter writer;

                FBOMWriter serializer { FBOMWriterConfig {} };

                if (FBOMResult err = serializer.Append(*m_meshData))
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
    }

    break;
    default:
        HYP_NOT_IMPLEMENTED_VOID();
    }
}

StreamedMeshData::StreamedMeshData()
    : StreamedDataBase(),
      m_streamedData(MakeRefCountedPtr<NullStreamedData>()),
      m_numVertices(0),
      m_numIndices(0)
{
}

StreamedMeshData::StreamedMeshData(const MeshData& meshData, ResourceHandle& outResourceHandle)
    : StreamedMeshData(StreamedDataState::LOADED, MeshData(meshData), outResourceHandle)
{
}

StreamedMeshData::StreamedMeshData(MeshData&& meshData, ResourceHandle& outResourceHandle)
    : StreamedMeshData(StreamedDataState::LOADED, std::move(meshData), outResourceHandle)
{
}

StreamedMeshData::~StreamedMeshData()
{
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    if (m_streamedData != nullptr)
    {
        m_streamedData->WaitForFinalization();
        m_streamedData.Reset();
    }
}

bool StreamedMeshData::IsInMemory_Internal() const
{
    HYP_MT_CHECK_READ(m_dataRaceDetector);

    return m_meshData.HasValue();
}

void StreamedMeshData::Unpage_Internal()
{
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    if (m_streamedData)
    {
        m_streamedData->Unpage();
    }

    m_meshData.Unset();
}

void StreamedMeshData::Load_Internal() const
{
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    Assert(m_streamedData != nullptr);
    m_streamedData->Load();

    if (!m_meshData.HasValue())
    {
        LoadMeshData(m_streamedData->GetByteBuffer());
    }
}

void StreamedMeshData::LoadMeshData(const ByteBuffer& byteBuffer) const
{
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    Assert(byteBuffer.Size() >= 3);
    Assert(byteBuffer.Data()[0] == 'H');
    Assert(byteBuffer.Data()[1] == 'Y');
    Assert(byteBuffer.Data()[2] == 'P');

    MemoryBufferedReaderSource source { byteBuffer.ToByteView() };
    BufferedReader reader { &source };

    if (!reader.IsOpen())
    {
        HYP_LOG(Streaming, Warning, "StreamedMeshData: Failed to open buffered reader for mesh data!");

        return;
    }

    HypData value;

    FBOMReader deserializer { FBOMReaderConfig {} };
    FBOMLoadContext context;

    if (FBOMResult err = deserializer.Deserialize(context, reader, value))
    {
        HYP_LOG(Streaming, Warning, "StreamedMeshData: Error deserializing mesh data: {}", err.message);
        return;
    }

    m_meshData = value.Get<MeshData>();

    HYP_LOG(Streaming, Debug, "StreamedMeshData: Loaded mesh data with {} vertices and {} indices on thread {}", m_meshData->vertices.Size(), m_meshData->indices.Size(),
        ThreadId::Current().GetName());

    if (m_meshData->vertices.Size() != m_numVertices)
    {
        HYP_LOG(Streaming, Warning, "StreamedMeshData: Vertex count mismatch! Expected {} vertices, but loaded data has {} vertices", m_numVertices, m_meshData->vertices.Size());
    }

    if (m_meshData->indices.Size() != m_numIndices)
    {
        HYP_LOG(Streaming, Warning, "StreamedMeshData: Index count mismatch! Expected {} indices, but loaded data has {} indices", m_numIndices, m_meshData->indices.Size());
    }
}

const MeshData& StreamedMeshData::GetMeshData() const
{
    // wait for loading tasks to complete on another thread
    WaitForTaskCompletion();

    if (m_streamedData)
    {
        m_streamedData->WaitForTaskCompletion();
    }

    HYP_MT_CHECK_READ(m_dataRaceDetector);

    AssertDebug(ResourceBase::IsInitialized(), "StreamedMeshData: Cannot get mesh data for uninitialized resource!");

    if (!m_meshData.HasValue())
    {
        static const MeshData defaultValue {};

        return defaultValue;
    }

    return m_meshData.Get();
}

} // namespace hyperion