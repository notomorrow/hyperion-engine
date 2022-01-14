#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <array>
#include <memory>

#include "../post_filter.h"
#include "../../texture_2D.h"
#include "../../../math/vector3.h"
#include "../../../math/vector2.h"
#include "../../../math/matrix4.h"

namespace apex {

class BloomFilter : public PostFilter {
public:
    BloomFilter();

    virtual void SetUniforms(Camera *cam) override;

private:
};

} // namespace apex

#endif
