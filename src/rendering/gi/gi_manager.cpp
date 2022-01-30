#include "gi_manager.h"
#include "../renderer.h"
#include "../shader_manager.h"
#include "../shaders/gi/gi_voxel_shader.h"
#include "../shaders/gi/gi_voxel_clear_shader.h"

namespace hyperion {
GIManager *GIManager::instance = nullptr;

GIManager *GIManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new GIManager();
    }

    return instance;
}

GIManager::GIManager()
{
}
} // namespace apex