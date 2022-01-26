#ifndef TREE_POPULATOR_H
#define TREE_POPULATOR_H

#include "populator.h"

namespace hyperion {

class TreePopulator : public Populator {
public:
    TreePopulator(
        Camera *camera,
        unsigned long seed = 555,
        double probability_factor = 0.35,
        float tolerance = 0.15f,
        float max_distance = 450.0f,
        float spread = 4.5f,
        int num_entities_per_chunk = 2,
        int num_patches = 2
    );
    ~TreePopulator();

    std::shared_ptr<Entity> CreateEntity(const Vector3 &position) const override;
};

} // namespace hyperion

#endif
