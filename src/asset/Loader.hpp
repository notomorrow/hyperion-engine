/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LOADER_HPP
#define HYPERION_LOADER_HPP

#include <core/io/ByteReader.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/utilities/Result.hpp>

#define HYP_LOADER_BUFFER_SIZE 2048

namespace hyperion {

class Engine;
class AssetManager;

struct LoaderState
{
    using Stream = BufferedReader;

    AssetManager* assetManager;
    String filepath;
    Stream stream;
};

class AssetLoadError final : public Error
{
public:
    enum ErrorCode : int
    {
        UNKNOWN = -1,
        ERR_NOT_FOUND = 1,
        ERR_NO_LOADER,
        ERR_EOF
    };

    AssetLoadError()
        : Error(),
          m_errorCode(UNKNOWN)
    {
    }

    template <auto MessageString>
    AssetLoadError(const StaticMessage& currentFunction, ValueWrapper<MessageString>, ErrorCode errorCode)
        : Error(currentFunction, ValueWrapper<MessageString>()),
          m_errorCode(errorCode)
    {
    }

    template <auto MessageString, class... Args>
    AssetLoadError(const StaticMessage& currentFunction, ValueWrapper<MessageString>, Args&&... args)
        : Error(currentFunction, ValueWrapper<MessageString>(), std::forward<Args>(args)...),
          m_errorCode(UNKNOWN)
    {
    }

    virtual ~AssetLoadError() override = default;

    HYP_FORCE_INLINE ErrorCode GetErrorCode() const
    {
        return m_errorCode;
    }

private:
    ErrorCode m_errorCode;
};

} // namespace hyperion

#endif
