#ifndef RENDERER_RESULT_H
#define RENDERER_RESULT_H

#include <util.h>
#include <system/debug.h>

namespace hyperion {
namespace renderer {
struct Result {
    static const Result OK;

    enum {
        RENDERER_OK = 0,
        RENDERER_ERR = 1,
        RENDERER_ERR_NEEDS_REALLOCATION = 2
    } result;

    const char *message;

    Result(decltype(result) result, const char *message = "")
        : result(result), message(message) {}
    Result(const Result &other)
        : result(other.result), message(other.message) {}

    HYP_FORCE_INLINE
    operator bool() const { return result == RENDERER_OK; }
};

#define HYPERION_RETURN_OK \
    do { \
        return ::hyperion::renderer::Result::OK; \
    } while (0)

#define HYPERION_PASS_ERRORS(result, out_result) \
    do { \
        ::hyperion::renderer::Result _result = (result); \
        if (!_result) (out_result) = _result; \
    } while (0)

#define HYPERION_BUBBLE_ERRORS(result) \
    do { \
        ::hyperion::renderer::Result _result = (result); \
        if (!_result) return _result; \
    } while (0)

#define HYPERION_IGNORE_ERRORS(result) \
    do { \
        ::hyperion::renderer::Result _result = (result); \
        (void)_result; \
    } while (0)

#define HYPERION_VK_CHECK(vk_result) \
    do { \
        if ((vk_result) != VK_SUCCESS) \
            return ::hyperion::renderer::Result(::hyperion::renderer::Result::RENDERER_ERR, #vk_result " != VK_SUCCESS"); \
    } while (0)

#define HYPERION_VK_PASS_ERRORS(vk_result, out_result) \
    do { \
        if ((vk_result) != VK_SUCCESS) (out_result) = ::hyperion::renderer::Result(::hyperion::renderer::Result::RENDERER_ERR, #vk_result " != VK_SUCCESS"); \
    } while (0)

#define HYPERION_VK_CHECK_MSG(vk_result, msg) \
    do { \
        if ((vk_result) != VK_SUCCESS) \
            return ::hyperion::renderer::Result(::hyperion::renderer::Result::RENDERER_ERR, msg ":\t" #vk_result " != VK_SUCCESS"); \
    } while (0)

} // namespace renderer
} // namespace hyperion

#endif