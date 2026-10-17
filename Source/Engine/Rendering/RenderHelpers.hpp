/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Math/Vector3.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderMemory.hpp>

#include <Core/Types.hpp>

#include <Core/Name/Name.hpp> // For StringHash

namespace Hyperion {

template <class AllocatorType>
class TCommandRecorder;

using CommandRecorder = TCommandRecorder<RenderAllocator>;

class MaterialParameters;

namespace RenderHelpers {

uint32 MipmapSize(uint32 srcSize, int lod);

// every backend supports at least this many compute groups along each dimension
static constexpr uint32 MaxComputeGroupsPerDimension = 65535;

Vec3u WrapComputeGroupCount(uint32 numGroups);

//////////////////////////
///// Shader Helpers /////
//////////////////////////

constexpr inline bool IsCubemapShader(StringHash shaderNameHash)
{
    constexpr StringHash CubemapShaderNames[] = {
        "DrawCubemap"_sh,
        "RenderSky"_sh
    };

    return shaderNameHash == CubemapShaderNames[0]
        || shaderNameHash == CubemapShaderNames[1];
}

constexpr inline bool IsGeometryPassFamily(StringHash shaderNameHash)
{
    constexpr StringHash GeometryPassShaderNames[] = {
        "GeometryPass"_sh,
        "Terrain"_sh
    };

    for (StringHash name : GeometryPassShaderNames)
    {
        if (shaderNameHash == name)
        {
            return true;
        }
    }

    return false;
}

constexpr inline bool IsTerrainShader(StringHash shaderNameHash)
{
    return shaderNameHash == "Terrain"_sh;
}

ENGINE_API bool IsEmissive(const MaterialParameters& materialParameters);

constexpr inline bool ShaderSupportsMorph(StringHash shaderNameHash)
{
    constexpr StringHash MorphShaderNames[] = {
        "Terrain"_sh,
        "DrawShadowMap"_sh
    };
    
    for (StringHash name : MorphShaderNames)
    {
        if (shaderNameHash == name)
        {
            return true;
        }
    }

    return false;
}

} // namespace RenderHelpers

class SingleTimeCommands
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    virtual ~SingleTimeCommands() = default;

    void Push(Proc<void(CommandRecorder&)>&& fn)
    {
        m_functions.PushBack(std::move(fn));
    }

    virtual RendererResult Execute() = 0;

protected:
    Array<Proc<void(CommandRecorder&)>, RenderAllocator> m_functions;
};

} // namespace Hyperion
