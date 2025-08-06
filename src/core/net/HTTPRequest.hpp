/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/containers/String.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Optional.hpp>

#include <core/functional/Proc.hpp>
#include <core/functional/Delegate.hpp>

#include <core/threading/Task.hpp>
#include <core/threading/Mutex.hpp>

#include <core/json/JSON.hpp>

#include <core/Types.hpp>

namespace hyperion {
namespace net {

enum class HTTPMethod
{
    GET,
    POST,
    PUT,
    PATCH,
    DELETE_
};

class HYP_API HTTPResponse
{
public:
    friend class HTTPRequest;

    HTTPResponse();
    HTTPResponse(const HTTPResponse& other) = delete;
    HTTPResponse& operator=(const HTTPResponse& other) = delete;
    HTTPResponse(HTTPResponse&& other) noexcept;
    HTTPResponse& operator=(HTTPResponse&& other) noexcept = delete;
    ~HTTPResponse();

    HYP_FORCE_INLINE int GetStatusCode() const
    {
        return m_statusCode;
    }

    HYP_FORCE_INLINE bool IsSuccess() const
    {
        return m_statusCode >= 200 && m_statusCode < 400;
    }

    HYP_FORCE_INLINE bool IsError() const
    {
        return m_statusCode >= 400;
    }

    HYP_FORCE_INLINE const ByteBuffer& ToByteBuffer() const
    {
        return m_body;
    }

    Optional<json::JSONValue> ToJSON() const;

    void OnDataReceived(Span<char> data);
    void OnComplete(int statusCode);

    Delegate<void, Span<char>> OnDataReceivedDelegate;
    Delegate<void, int> OnCompleteDelegate;

private:
    int m_statusCode;
    ByteBuffer m_body;
    mutable Mutex m_mutex;
};

class HYP_API HTTPRequest
{
public:
    HTTPRequest(const String& url, HTTPMethod method = HTTPMethod::GET);
    HTTPRequest(const String& url, const json::JSONValue& body, HTTPMethod method = HTTPMethod::GET);
    HTTPRequest(const HTTPRequest& other);
    HTTPRequest& operator=(const HTTPRequest& other);
    HTTPRequest(HTTPRequest&& other) noexcept;
    HTTPRequest& operator=(HTTPRequest&& other) noexcept;
    ~HTTPRequest();

    HYP_FORCE_INLINE const String& GetURL() const
    {
        return m_url;
    }

    HYP_FORCE_INLINE HTTPMethod GetMethod() const
    {
        return m_method;
    }

    HYP_NODISCARD Task<HTTPResponse> Send();

private:
    String m_url;
    HTTPMethod m_method;
    ByteBuffer m_body;
    String m_contentType;
};

} // namespace net

using net::HTTPMethod;
using net::HTTPRequest;
using net::HTTPResponse;

} // namespace hyperion
