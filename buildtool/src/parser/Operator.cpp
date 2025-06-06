#include <parser/Operator.hpp>

namespace hyperion::buildtool {

const Operator::OperatorMap Operator::s_binary_operators = {
    { "+", Operator(OP_add, 13, ARITHMETIC, false, true) },
    { "-", Operator(OP_subtract, 13, ARITHMETIC, false, true) },
    { "*", Operator(OP_multiply, 14, ARITHMETIC, false, true) },
    { "/", Operator(OP_divide, 14, ARITHMETIC, false, true) },
    { "%", Operator(OP_modulus, 14, ARITHMETIC, false, true) },

    // Bitwise operators
    { "&", Operator(OP_bitwise_and, 9, BITWISE, false, true) },
    { "^", Operator(OP_bitwise_xor, 8, BITWISE, false, true) },
    { "|", Operator(OP_bitwise_or, 7, BITWISE, false, true) },
    { "<<", Operator(OP_bitshift_left, 12, BITWISE, false, true) },
    { ">>", Operator(OP_bitshift_right, 12, BITWISE, false, true) },

    // Logical operators
    { "&&", Operator(OP_logical_and, 6, LOGICAL, false, true) },
    { "||", Operator(OP_logical_or, 5, LOGICAL, false, true) },

    // Comparison operators
    { "==", Operator(OP_equals, 10, COMPARISON, false, true) },
    { "!=", Operator(OP_not_eql, 10, COMPARISON, false, true) },
    { "<", Operator(OP_less, 11, COMPARISON, false, true) },
    { ">", Operator(OP_greater, 11, COMPARISON, false, true) },
    { "<=", Operator(OP_less_eql, 11, COMPARISON, false, true) },
    { ">=", Operator(OP_greater_eql, 11, COMPARISON, false, true) },

    // Assignment operators
    { "=", Operator(OP_assign, 3, ASSIGNMENT, true, false) },
    { "+=", Operator(OP_add_assign, 3, ASSIGNMENT | ARITHMETIC, true, true) },
    { "-=", Operator(OP_subtract_assign, 3, ASSIGNMENT | ARITHMETIC, true, true) },
    { "*=", Operator(OP_multiply_assign, 3, ASSIGNMENT | ARITHMETIC, true, true) },
    { "/=", Operator(OP_divide_assign, 3, ASSIGNMENT | ARITHMETIC, true, true) },
    { "%=", Operator(OP_modulus_assign, 3, ASSIGNMENT | ARITHMETIC, true, true) },
    { "^=", Operator(OP_bitwise_xor_assign, 3, ASSIGNMENT | BITWISE, true, true) },
    { "&=", Operator(OP_bitwise_and_assign, 3, ASSIGNMENT | BITWISE, true, true) },
    { "|=", Operator(OP_bitwise_or_assign, 3, ASSIGNMENT | BITWISE, true, true) },
};

const Operator::OperatorMap Operator::s_unary_operators = {
    // Unary operators
    { "!", Operator(OP_logical_not, 0, LOGICAL | PREFIX, false, true) },
    { "-", Operator(OP_negative, 0, ARITHMETIC | PREFIX, false, true) },
    { "+", Operator(OP_positive, 0, ARITHMETIC | PREFIX, false, true) },
    { "~", Operator(OP_bitwise_complement, 0, BITWISE | PREFIX, false, true) },
    { "++", Operator(OP_increment, 0, ASSIGNMENT | ARITHMETIC | POSTFIX | PREFIX, true, true) },
    { "--", Operator(OP_decrement, 0, ASSIGNMENT | ARITHMETIC | POSTFIX | PREFIX, true, true) }
};

Operator::Operator(
    Operators op_type,
    int precedence,
    int type,
    bool modifies_value,
    bool supports_overloading)
    : m_op_type(op_type),
      m_precedence(precedence),
      m_type(type),
      m_modifies_value(modifies_value),
      m_supports_overloading(supports_overloading)
{
}

Operator::Operator(const Operator& other)
    : m_op_type(other.m_op_type),
      m_precedence(other.m_precedence),
      m_type(other.m_type),
      m_modifies_value(other.m_modifies_value),
      m_supports_overloading(other.m_supports_overloading)
{
}

String Operator::LookupStringValue() const
{
    const OperatorMap* map = &Operator::s_binary_operators;

    if (IsUnary())
    {
        map = &Operator::s_unary_operators;
    }

    for (const auto& it : *map)
    {
        if (it.second.GetOperatorType() == m_op_type)
        {
            return it.first;
        }
    }

    return "??";
}

bool Operator::IsBinaryOperator(UTF8StringView str, OperatorTypeBits match_bits)
{
    const auto it = s_binary_operators.FindAs(str);

    if (it == s_binary_operators.end())
    {
        return false;
    }

    if (match_bits == 0)
    {
        return true;
    }

    return bool(it->second.GetType() & match_bits);
}

bool Operator::IsBinaryOperator(UTF8StringView str, const Operator*& out)
{
    const auto it = s_binary_operators.FindAs(str);

    if (it == s_binary_operators.end())
    {
        return false;
    }

    out = &it->second;

    return true;
}

bool Operator::IsBinaryOperator(UTF8StringView str, OperatorTypeBits match_bits, const Operator*& out)
{
    const auto it = s_binary_operators.FindAs(str);

    if (it == s_binary_operators.end())
    {
        return false;
    }

    if (match_bits == 0)
    {
        out = &it->second;

        return true;
    }

    if (it->second.GetType() & match_bits)
    {
        out = &it->second;

        return true;
    }

    return false;
}

bool Operator::IsUnaryOperator(UTF8StringView str, OperatorTypeBits match_bits)
{
    const auto it = s_unary_operators.FindAs(str);

    if (it == s_unary_operators.end())
    {
        return false;
    }

    if (match_bits == 0)
    {
        return true;
    }

    return bool(it->second.GetType() & match_bits);
}

bool Operator::IsUnaryOperator(UTF8StringView str, const Operator*& out)
{
    const auto it = s_unary_operators.FindAs(str);

    if (it == s_unary_operators.end())
    {
        return false;
    }

    out = &it->second;

    return true;
}

bool Operator::IsUnaryOperator(UTF8StringView str, OperatorTypeBits match_bits, const Operator*& out)
{
    const auto it = s_unary_operators.FindAs(str);

    if (it == s_unary_operators.end())
    {
        return false;
    }

    if (match_bits == 0)
    {
        out = &it->second;

        return true;
    }

    if (it->second.GetType() & match_bits)
    {
        out = &it->second;

        return true;
    }

    return false;
}

const Operator* Operator::FindBinaryOperator(Operators op)
{
    for (auto& it : s_binary_operators)
    {
        if (it.second.GetOperatorType() == op)
        {
            return &it.second;
        }
    }

    return nullptr;
}

const Operator* Operator::FindUnaryOperator(Operators op)
{
    for (auto& it : s_unary_operators)
    {
        if (it.second.GetOperatorType() == op)
        {
            return &it.second;
        }
    }

    return nullptr;
}

} // namespace hyperion::buildtool