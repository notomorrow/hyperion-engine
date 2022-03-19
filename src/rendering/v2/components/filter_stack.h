#ifndef HYPERION_V2_FILTER_STACK_H
#define HYPERION_V2_FILTER_STACK_H

#include "filter.h"

#include <rendering/mesh.h>
#include <rendering/backend/renderer_frame.h>

#include <memory>
#include <utility>

namespace hyperion::v2 {

using renderer::Frame;

class Engine;

class FilterStack {
public:
    FilterStack();
    FilterStack(const FilterStack &) = delete;
    FilterStack &operator=(const FilterStack &) = delete;
    ~FilterStack();

    void AddFilter(std::unique_ptr<Filter> &&filter);

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    //void BuildPipelines(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary_command_buffer, uint32_t frame_index);

private:
    std::vector<std::unique_ptr<Filter>> m_filters;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_FILTER_STACK_H

