#include "scene_manager.h"

namespace hyperion {

SceneManager *SceneManager::instance = nullptr;

SceneManager *SceneManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new SceneManager();
    }

    return instance;
}

SceneManager::SceneManager()
    : m_octree(new Octree(BoundingBox(-250, 250)))// TODO
{
}

SceneManager::~SceneManager()
{
    delete m_octree;
}
} // namespace hyperion
