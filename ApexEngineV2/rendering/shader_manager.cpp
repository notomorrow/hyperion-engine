#include "shader_manager.h"

namespace apex {

ShaderManager *ShaderManager::instance = nullptr;

ShaderManager *ShaderManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new ShaderManager();
    }
    return instance;
}

} // namespace apex