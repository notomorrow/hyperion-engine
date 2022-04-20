#include "renderer_pipeline.h"

#include <system/debug.h>

namespace hyperion {
namespace renderer {

Pipeline::Pipeline()
    : pipeline{},
      layout{},
      push_constants{}
{
}

Pipeline::~Pipeline()
{
    AssertThrowMsg(pipeline == nullptr, "Expected pipeline to have been destroyed");
    AssertThrowMsg(layout == nullptr, "Expected layout to have been destroyed");
}

} // namespace renderer
} // namespace hyperion