#ifndef GRASS_POPULATOR_H
#define GRASS_POPULATOR_H

#include "populator.h"

namespace hyperion {

class GrassPopulator : public Populator {
public:
    GrassPopulator(
        Camera *camera,
        unsigned long seed = 12345,
        double probability_factor = 0.45,
        float tolerance = 0.1f,
        float max_distance = 20.0f,
        float spread = 1.5f,
        int num_entities_per_chunk = 4,
        int num_patches = 5
    );
    ~GrassPopulator();

    std::shared_ptr<Node> CreateEntity(const Vector3 &position) const override;

protected:
    virtual std::shared_ptr<Control> CloneImpl() override;
};

} // namespace hyperion

#endif
