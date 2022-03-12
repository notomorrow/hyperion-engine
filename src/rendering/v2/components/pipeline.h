#ifndef HYPERION_V2_PIPELINE_H
#define HYPERION_V2_PIPELINE_H

#include "base.h"

#include <rendering/backend/renderer_pipeline.h>

#include <memory>

namespace hyperion::v2 {

class Pipeline : public BaseComponent<renderer::Pipeline> {
public:
    explicit Pipeline(renderer::Pipeline::ConstructionInfo &&construction_info);
    Pipeline(const Pipeline &) = delete;
    Pipeline &operator=(const Pipeline &) = delete;
    ~Pipeline();

    void Create(Engine *engine);
    void Destroy(Engine *engine);
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_PIPELINE_H

