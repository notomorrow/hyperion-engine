#ifndef RENDERER_RESULT_H
#define RENDERER_RESULT_H

namespace hyperion {

struct RendererResult {
    enum {
        RENDERER_OK = 0,
        RENDERER_ERR = 1,
        RENDERER_ERR_NEEDS_REALLOCATION = 2
    } result;

    const char *message;

    RendererResult(decltype(result) result, const char *message = "")
        : result(result), message(message) {}
    RendererResult(const RendererResult &other)
        : result(other.result), message(other.message) {}

    inline operator bool() const { return result == RENDERER_OK; }
};

} // namespace hyperion

#endif