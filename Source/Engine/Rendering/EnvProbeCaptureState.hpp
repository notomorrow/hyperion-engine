/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Name/Name.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Rendering/RenderMemory.hpp>

#include <Scene/BakedLighting/SphericalHarmonics.hpp>

namespace Hyperion {

class EnvProbe;
class Texture;

struct ENGINE_API EnvProbeCaptureState
{
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    EnvProbeCaptureState(EnvProbe* envProbe, Name layerName);
    ~EnvProbeCaptureState();

    EnvProbeCaptureState(const EnvProbeCaptureState& other) = delete;
    EnvProbeCaptureState& operator=(const EnvProbeCaptureState& other) = delete;

    void Begin();
    void End(bool commitResult);

    Name layerName;                             //!< layer being baked for
    Handle<Texture> texture;                    //!< cubemap capture target
    Handle<Texture> visibilityTexture;          //!< visibility capture target
    SphericalHarmonicsData sphericalHarmonics;

private:
    friend class EnvProbe;

    EnvProbe* m_envProbe;
};

} // namespace Hyperion
