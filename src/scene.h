#ifndef SCENE_H
#define SCENE_H

#include "octree.h"
#include "scene_node.h"

namespace hyperion {

class Scene {
public:
private:
    Octree<std::shared_ptr<SceneNode>> *m_octree;
};

} // namespace hyperion

#endif