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

RC<StreamedMeshData> StreamedMeshData::FromMeshData(MeshData mesh_data)
{
    return MakeRefCountedPtr<StreamedMeshData>(std::move(mesh_data));
}

StreamedMeshData::StreamedMeshData(StreamedDataState initial_state, MeshData mesh_data)
    : StreamedData(initial_state),
      m_streamed_data(nullptr),
      m_num_vertices(mesh_data.vertices.Size()),
      m_num_indices(mesh_data.indices.Size())
{
    switch (initial_state) {
    case StreamedDataState::NONE:
        m_streamed_data = MakeRefCountedPtr<NullStreamedData>();

        break;
    case StreamedDataState::LOADED: // fallthrough
    case StreamedDataState::UNPAGED:
        m_mesh_data.Set(std::move(mesh_data));

        m_streamed_data = MakeRefCountedPtr<MemoryStreamedData>(m_mesh_data->GetHashCode(), StreamedDataState::UNPAGED, [this](HashCode hc, ByteBuffer &out) -> bool
        {
            if (!m_mesh_data) {
                return false;
            }

            MemoryByteWriter writer;

            fbom::FBOMWriter serializer { fbom::FBOMWriterConfig { } };
            
            if (fbom::FBOMResult err = serializer.Append(*m_mesh_data)) {
                HYP_LOG(Streaming, Error, "Failed to write streamed data: {}", err.message);

                return false;
            }

            if (fbom::FBOMResult err = serializer.Emit(&writer)) {
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

StreamedMeshData::StreamedMeshData()
    : StreamedMeshData(StreamedDataState::NONE, { })
{
}

StreamedMeshData::StreamedMeshData(const MeshData &mesh_data)
    : StreamedMeshData(StreamedDataState::LOADED, mesh_data)
{
}

StreamedMeshData::StreamedMeshData(MeshData &&mesh_data)
    : StreamedMeshData(StreamedDataState::LOADED, std::move(mesh_data))
{
}

StreamedMeshData::~StreamedMeshData()
{
    if (m_streamed_data != nullptr) {
        m_streamed_data->WaitForCompletion();
        m_streamed_data.Reset();
    }
}

bool StreamedMeshData::IsInMemory_Internal() const
{
    return m_mesh_data.HasValue();
}

void StreamedMeshData::Unpage_Internal()
{
    m_mesh_data.Unset();

    if (!m_streamed_data) {
        return;
    }

    m_streamed_data->Unpage();
}

const ByteBuffer &StreamedMeshData::Load_Internal() const
{
    AssertThrow(m_streamed_data != nullptr);

    const ByteBuffer &byte_buffer = m_streamed_data->Load();

    if (!m_mesh_data.HasValue()) {
        LoadMeshData(byte_buffer);
    }

    return byte_buffer;
}

void StreamedMeshData::LoadMeshData(const ByteBuffer &byte_buffer) const
{
    AssertThrow(byte_buffer.Size() >= 3);
    AssertThrow(byte_buffer.Data()[0] == 'H');
    AssertThrow(byte_buffer.Data()[1] == 'Y');
    AssertThrow(byte_buffer.Data()[2] == 'P');

    m_mesh_data.Unset();

    BufferedReader reader(MakeRefCountedPtr<MemoryBufferedReaderSource>(byte_buffer.ToByteView()));

    if (!reader.IsOpen()) {
        return;
    }
    
    HypData value;

    fbom::FBOMReader deserializer { fbom::FBOMReaderConfig { } };
    fbom::FBOMLoadContext context;

    if (fbom::FBOMResult err = deserializer.Deserialize(context, reader, value)) {
        HYP_LOG(Streaming, Warning, "StreamedMeshData: Error deserializing mesh data: {}", err.message);
        return;
    }

    m_mesh_data = value.Get<MeshData>();

    if (m_mesh_data->vertices.Size() != m_num_vertices) {
        HYP_LOG(Streaming, Warning, "StreamedMeshData: Vertex count mismatch! Expected {} vertices, but loaded data has {} vertices", m_num_vertices, m_mesh_data->vertices.Size());
    }

    if (m_mesh_data->indices.Size() != m_num_indices) {
        HYP_LOG(Streaming, Warning, "StreamedMeshData: Index count mismatch! Expected {} indices, but loaded data has {} indices", m_num_indices, m_mesh_data->indices.Size());
    }
}

const MeshData &StreamedMeshData::GetMeshData() const
{
    static const MeshData default_value { };

    if (!m_mesh_data.HasValue()) {
        return default_value;
    }

    return m_mesh_data.Get();
}

} // namespace hyperion