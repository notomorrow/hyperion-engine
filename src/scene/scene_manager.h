#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include "octree.h"

namespace hyperion {
class Node;
class SceneManager {
public:
    static SceneManager *instance;
    static SceneManager *GetInstance();

    SceneManager();
    ~SceneManager();

    inline Octree *GetOctree() { return m_octree; }
    inline const Octree *GetOctree() const { return m_octree; }

private:
    Octree *m_octree;
};

} // namespace hyperion

#endif
