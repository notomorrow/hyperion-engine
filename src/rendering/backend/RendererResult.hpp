/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_RESULT_HPP
#define HYPERION_BACKEND_RENDERER_RESULT_HPP

#include <core/Defines.hpp>

#include <core/utilities/Result.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {
class RendererError final : public Error
{
public:
    RendererError()
        : Error(),
          m_errorCode(0)
    {
    }

    template <auto MessageString>
    RendererError(const StaticMessage& currentFunction, ValueWrapper<MessageString>)
        : Error(currentFunction, ValueWrapper<MessageString>()),
          m_errorCode(0)
    {
    }

    template <auto MessageString, class... Args>
    RendererError(const StaticMessage& currentFunction, ValueWrapper<MessageString>, int errorCode, Args&&... args)
        : Error(currentFunction, ValueWrapper<MessageString>(), std::forward<Args>(args)...),
          m_errorCode(errorCode)
    {
    }

    virtual ~RendererError() override = default;

    HYP_FORCE_INLINE int GetErrorCode() const
    {
        return m_errorCode;
    }

private:
    int m_errorCode;
};

using RendererResult = TResult<void, RendererError>;

#define HYPERION_RETURN_OK                    \
    do                                        \
    {                                         \
        return ::hyperion::RendererResult {}; \
    }                                         \
    while (0)

#define HYPERION_PASS_ERRORS(result, outResult)       \
    do                                                 \
    {                                                  \
        ::hyperion::RendererResult _result = (result); \
        if ((outResult) && !_result)                  \
            (outResult) = _result;                    \
    }                                                  \
    while (0)

#define HYPERION_BUBBLE_ERRORS(result)                 \
    do                                                 \
    {                                                  \
        ::hyperion::RendererResult _result = (result); \
        if (!_result)                                  \
            return _result;                            \
    }                                                  \
    while (0)

#define HYPERION_IGNORE_ERRORS(result)                 \
    do                                                 \
    {                                                  \
        ::hyperion::RendererResult _result = (result); \
        (void)_result;                                 \
    }                                                  \
    while (0)

#define HYPERION_ASSERT_RESULT(result)                                                                                              \
    do                                                                                                                              \
    {                                                                                                                               \
        auto _result = (result);                                                                                                    \
        AssertThrowMsg(_result, "[Error Code: %d]  %s", _result.GetError().GetErrorCode(), _result.GetError().GetMessage().Data()); \
    }                                                                                                                               \
    while (0)

} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererResult.hpp>
#else
#error Unsupported rendering backend
#endif

#endif
