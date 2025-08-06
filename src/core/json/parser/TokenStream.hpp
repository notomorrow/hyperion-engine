/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/json/parser/Token.hpp>

#include <core/debug/Debug.hpp>

#include <core/Types.hpp>

namespace hyperion::json {

struct TokenStreamInfo
{
    String filepath;

    TokenStreamInfo(const String& filepath)
        : filepath(filepath)
    {
    }

    TokenStreamInfo(const TokenStreamInfo& other)
        : filepath(other.filepath)
    {
    }
};

class TokenStream
{
public:
    TokenStream(const TokenStreamInfo& info);
    TokenStream(const TokenStream& other) = delete;

    Token Peek(int n = 0) const
    {
        SizeType pos = m_position + n;

        if (pos >= m_tokens.Size())
        {
            return Token::empty;
        }

        return m_tokens[pos];
    }

    void Push(const Token& token)
    {
        m_tokens.PushBack(token);
    }

    bool HasNext() const
    {
        return m_position < m_tokens.Size();
    }

    Token Next()
    {
        if (m_position < m_tokens.Size())
        {
            return m_tokens[m_position++];
        }

        return Token::empty;
    }

    Token Last() const
    {
        if (m_tokens.Any())
        {
            return m_tokens.Back();
        }

        return Token::empty;
    }

    SizeType GetSize() const
    {
        return m_tokens.Size();
    }

    SizeType GetPosition() const
    {
        return m_position;
    }

    const TokenStreamInfo& GetInfo() const
    {
        return m_info;
    }

    void SetPosition(SizeType position)
    {
        m_position = position;
    }

    bool Eof() const
    {
        return m_position >= m_tokens.Size();
    }

    Array<Token> m_tokens;
    SizeType m_position;

private:
    TokenStreamInfo m_info;
};

} // namespace hyperion::json
