/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Volume.hpp>

namespace Hyperion {

class Texture;

HYP_CLASS()
class ENGINE_API FogVolume final : public VolumeBase
{
    HYP_OBJECT_BODY(FogVolume);

public:
    static constexpr uint32 MaxVolumeTextureExtent = 64;
    static constexpr uint32 MaxNoiseTextureExtent = 32;

    FogVolume();

    explicit FogVolume(const BoundingBox& localBounds);

    FogVolume(const FogVolume&) = delete;
    FogVolume& operator=(const FogVolume&) = delete;

    ~FogVolume() override;

    HYP_METHOD(Property = "VolumeTexture")
    HYP_FORCE_INLINE const Handle<Texture>& GetVolumeTexture() const
    {
        return m_volumeTexture;
    }

    HYP_METHOD(Property = "VolumeTexture")
    void SetVolumeTexture(const Handle<Texture>& volumeTexture);

    HYP_METHOD(Property = "NoiseTexture")
    HYP_FORCE_INLINE const Handle<Texture>& GetNoiseTexture() const
    {
        return m_noiseTexture;
    }

    HYP_METHOD(Property = "NoiseTexture")
    void SetNoiseTexture(const Handle<Texture>& noiseTexture);

    HYP_METHOD()
    void SetTextures(
        const Handle<Texture>& volumeTexture,
        const Handle<Texture>& noiseTexture);

    void UpdateRenderProxy(struct RenderProxyFogVolume* proxy);

    //-- Per-layer stuff

    static Name GetVolumeTexturePropertyName()
    {
        return NAME("VolumeTexture");
    }

    static Name GetNoiseTexturePropertyName()
    {
        return NAME("NoiseTexture");
    }

    static Name BuildVolumeTextureName(Name volumeName, Name layerName);
    static Name BuildNoiseTextureName(Name volumeName, Name layerName);

    Handle<Texture> GetVolumeTextureForLayer(Name layerName) const;
    Handle<Texture> GetNoiseTextureForLayer(Name layerName) const;

    void SetTexturesForLayer(
        const Handle<Texture>& volumeTexture,
        const Handle<Texture>& noiseTexture,
        Name layerName);

#ifdef HYP_EDITOR
    HYP_METHOD(EditorOnly)
    Array<Name> GetBakedLayerNames() const;
#endif // HYP_EDITOR

#ifdef HYP_EDITOR
    HYP_METHOD(EditorOnly, EditorAction = "Bake Fog Texture")
    void Rebake();
#endif

private:
    HYP_FIELD(Property = "VolumeTexture")
    Handle<Texture> m_volumeTexture;

    HYP_FIELD(Property = "NoiseTexture")
    Handle<Texture> m_noiseTexture;
};

} // namespace Hyperion
