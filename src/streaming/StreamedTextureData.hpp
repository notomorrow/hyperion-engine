/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <streaming/StreamedData.hpp>

#include <rendering/RenderStructs.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/containers/Array.hpp>
#include <core/utilities/Optional.hpp>

#include <Types.hpp>

namespace hyperion {

class HYP_API StreamedTextureData final : public StreamedDataBase
{
    StreamedTextureData(StreamedDataState initialState, TextureData textureData, ResourceHandle& outResourceHandle);

public:
    StreamedTextureData();
    StreamedTextureData(const TextureData& textureData, ResourceHandle& outResourceHandle);
    StreamedTextureData(TextureData&& textureData, ResourceHandle& outResourceHandle);

    virtual ~StreamedTextureData() override = default;

    const TextureData& GetTextureData() const;
    void SetTextureData(TextureData&& textureData);

    const TextureDesc& GetTextureDesc() const;
    void SetTextureDesc(const TextureDesc& textureDesc);

    HYP_FORCE_INLINE SizeType GetBufferSize() const
    {
        return m_bufferSize;
    }

    virtual HashCode GetDataHashCode() const override
    {
        return m_streamedData ? m_streamedData->GetDataHashCode() : HashCode(0);
    }

protected:
    virtual bool IsInMemory_Internal() const override;

    virtual void Load_Internal() const override;
    virtual void Unpage_Internal() override;

private:
    void LoadTextureData(const ByteBuffer& byteBuffer) const;

    RC<StreamedDataBase> m_streamedData;

    mutable Optional<TextureData> m_textureData;
    mutable TextureDesc m_textureDesc;
    mutable SizeType m_bufferSize;
};

} // namespace hyperion
