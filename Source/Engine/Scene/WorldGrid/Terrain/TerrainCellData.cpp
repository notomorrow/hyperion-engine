/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainCellData.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/BlobStorage.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Memory/Memory.hpp>

#include <TerrainCellData.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

#pragma region TerrainCellData

TerrainCellData::TerrainCellData()
    : AssetObject()
{
}

TerrainCellData::TerrainCellData(Name name, const Vec2i& coord, const Vec3u& extent)
    : AssetObject(name),
      coord(coord),
      extent(extent)
{
}

TerrainCellData::~TerrainCellData()
{
    FreeBlobData(m_sculptDelta);
}

void TerrainCellData::Init()
{
    AssetObject::Init();
}

void TerrainCellData::SetSculptDelta(ConstByteView view)
{
    if (view.Size() == 0 && m_sculptDelta.size == 0)
    {
        return;
    }

    FreeBlobData(m_sculptDelta);

    if (view.Size() != 0)
    {
        AllocateBlobData(m_sculptDelta, view.Data(), view.Size(), 1);
    }

    MarkDirty();
}

ConstByteView TerrainCellData::GetSculptDelta() const
{
    if (m_sculptDelta.raw == nullptr || m_sculptDelta.size == 0)
    {
        return ConstByteView();
    }

    return ConstByteView(reinterpret_cast<const ubyte*>(m_sculptDelta.raw), m_sculptDelta.size);
}

Span<float> TerrainCellData::EnsureSculptDelta(uint32 numVertices)
{
    const size_t requiredSize = size_t(numVertices) * sizeof(float);

    if (m_sculptDelta.size < requiredSize)
    {
        // allocate new if we're out.

        FreeBlobData(m_sculptDelta);
        AllocateBlobData(m_sculptDelta, nullptr, requiredSize, alignof(float));

        if (m_sculptDelta.raw != nullptr)
        {
            // new allocation, start it zeroed
            Memory::Zero(m_sculptDelta.raw, requiredSize);
        }
    }

    // Bad sculpt data! BAD!
    Assert(m_sculptDelta.raw && m_sculptDelta.size >= requiredSize);

    if (!m_sculptDelta.raw || m_sculptDelta.size < requiredSize)
    {
        return Span<float>();
    }

    return Span<float>(reinterpret_cast<float*>(m_sculptDelta.raw), numVertices);
}

void TerrainCellData::PageBlobData()
{
    if (IsTransient() || !IsRegistered())
    {
        return;
    }

    Handle<AssetRegistry> registry = GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        return;
    }

    if (m_sculptDelta.raw == nullptr
        && m_sculptDelta.key
        && m_sculptDelta.size != 0)
    {
        if (PageBlobDataFromStorage(m_sculptDelta))
        {
            return;
        }

        const Name blobKey = m_sculptDelta.key;
        const uint64 expectedSize = m_sculptDelta.size;

        FileByteReader stream { registry->GetRootPath() / AssetBuckets::Terrain.GetName() / (String(*GetName()) + ".TSD.raw.blob") };

        if (!stream.Eof())
        {
            if (stream.Max() != expectedSize)
            {
                HYP_LOG(WorldGrid, Error, "Local blob data for terrain cell data asset '{}' is {} bytes but the manifest expects {}, ignoring it",
                        GetName(), stream.Max(), expectedSize);

                return;
            }

            ByteBuffer buffer = stream.Read(stream.Max());

            AllocateBlobData(m_sculptDelta, buffer.Data(), buffer.Size(), 1);
            m_sculptDelta.key = blobKey;

            return;
        }

        m_sculptDelta.readOnly = true;
    }
}

void TerrainCellData::UnpageBlobData()
{
    AssetObject::UnpageBlobData();

    AssertBlobDataPersisted(m_sculptDelta);

    if (!m_sculptDelta.readOnly)
    {
        FreeBlobData(m_sculptDelta);
    }

    m_sculptDelta.raw = nullptr;
}

#pragma endregion TerrainCellData

} // namespace Hyperion
