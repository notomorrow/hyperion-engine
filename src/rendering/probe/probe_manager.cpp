#include "probe_manager.h"
#include "../renderer.h"
#include "../shader_manager.h"

namespace hyperion {
const int ProbeManager::voxel_map_size = 128;
const float ProbeManager::voxel_map_scale = 3.0f;
const int ProbeManager::voxel_map_num_mipmaps = 7;

ProbeManager *ProbeManager::instance = nullptr;

ProbeManager *ProbeManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new ProbeManager();
    }

    return instance;
}

ProbeManager::ProbeManager()
{
    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties()
            .Define("VCT_MAP_SIZE", voxel_map_size)
            .Define("VCT_NUM_MAPS", 6)
            .Define("VCT_SCALE", voxel_map_scale)
            .Define("VCT_GEOMETRY_SHADER", false)
            .Define("PROBE_RENDER_TEXTURES", true)
            .Define("PROBE_RENDER_SHADING", true)
    );
}
} // namespace apex