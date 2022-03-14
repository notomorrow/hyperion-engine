#ifndef HYPERION_V2_FILTER_STACK_H
#define HYPERION_V2_FILTER_STACK_H

#include "render_pass.h"
#include "framebuffer.h"
#include "shader.h"
#include "filter.h"

#include <rendering/mesh.h>
#include <rendering/backend/renderer_frame_handler.h>

#include <memory>

namespace hyperion::v2 {

class FilterStack {
public:
    FilterStack();
    FilterStack(const FilterStack &) = delete;
    FilterStack &operator=(const FilterStack &) = delete;
    ~FilterStack();

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame, uint32_t frame_index);

//private:
    std::vector<std::unique_ptr<Filter>> m_filters;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_FILTER_STACK_H

