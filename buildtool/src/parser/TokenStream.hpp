/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_TOKEN_STREAM_HPP
#define HYPERION_BUILDTOOL_TOKEN_STREAM_HPP

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <parser/Token.hpp>

#include <core/debug/Debug.hpp>

#include <core/Types.hpp>

namespace hyperion::buildtool {

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

    TokenStream(const TokenStream& other);
    TokenStream& operator=(const TokenStream& other);

    TokenStream(TokenStream&& other) noexcept;
    TokenStream& operator=(TokenStream&& other) noexcept;

    Token Peek(int n = 0) const
    {
        SizeType pos = m_position + n;

        if (pos >= m_tokens.Size())
        {
            return Token::EMPTY;
        }

        return m_tokens[pos];
    }

    void Push(const Token& token, bool insertAtPosition = false)
    {
        if (!insertAtPosition || m_tokens.Size() <= m_position)
        {
            m_tokens.PushBack(token);
        }
        else
        {
            auto it = m_tokens.Begin() + m_position;

            m_tokens.Insert(it, token);
        }
    }

    void Pop()
    {
        Assert(m_position < m_tokens.Size());

        m_tokens.Erase(m_tokens.Begin() + m_position);
    }

    bool HasNext() const
    {
        return m_position < m_tokens.Size();
    }

    Token Next()
    {
        Assert(m_position < m_tokens.Size());
        return m_tokens[m_position++];
    }

    Token Last() const
    {
        Assert(!m_tokens.Empty());
        return m_tokens.Back();
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

    void Rewind(SizeType n)
    {
        Assert(n <= m_position);

        m_position -= n;
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

} // namespace hyperion::buildtool

#endif
