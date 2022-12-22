#ifndef TOKEN_STREAM_HPP
#define TOKEN_STREAM_HPP

#include <script/compiler/Token.hpp>
#include <system/Debug.hpp>
#include <Types.hpp>

#include <vector>

namespace hyperion::compiler {

struct TokenStreamInfo
{
    std::string filepath;

    TokenStreamInfo(const std::string &filepath)
        : filepath(filepath)
    {
    }

    TokenStreamInfo(const TokenStreamInfo &other)
        : filepath(other.filepath)
    {
    }
};

class TokenStream
{
public:
    TokenStream(const TokenStreamInfo &info);
    TokenStream(const TokenStream &other) = delete;
    
    Token Peek(int n = 0) const
    {
        SizeType pos = m_position + n;

        if (pos >= m_tokens.size()) {
            return Token::EMPTY;
        }

        return m_tokens[pos];
    }

    void Push(const Token &token) { m_tokens.push_back(token); }
    bool HasNext() const { return m_position < m_tokens.size(); }
    Token Next() { AssertThrow(m_position < m_tokens.size()); return m_tokens[m_position++]; }
    Token Last() const { AssertThrow(!m_tokens.empty()); return m_tokens.back(); }
    SizeType GetSize() const { return m_tokens.size(); }
    SizeType GetPosition() const { return m_position; }
    const TokenStreamInfo &GetInfo() const { return m_info; }
    void SetPosition(SizeType position) { m_position = position; }
    bool Eof() const { return m_position >= m_tokens.size(); }

    std::vector<Token> m_tokens;
    SizeType m_position;

private:
    TokenStreamInfo m_info;
};

} // namespace hyperion::compiler

#endif
