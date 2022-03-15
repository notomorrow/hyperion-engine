#ifndef HYPERION_V2_PIPELINE_H
#define HYPERION_V2_PIPELINE_H

#include "base.h"
#include "simple_wrapped.h"

#include <rendering/backend/renderer_graphics_pipeline.h>
#include <rendering/backend/renderer_compute_pipeline.h>

namespace hyperion::v2 {

using Pipeline = SimpleWrapped<renderer::GraphicsPipeline>;

} // namespace hyperion::v2

#endif // !HYPERION_V2_PIPELINE_H

