#include <script/compiler/Scope.hpp>

namespace hyperion::compiler {

Scope::Scope()
    : m_scopeType(SCOPE_TYPE_NORMAL),
      m_scopeFlags(0)
{
}

Scope::Scope(ScopeType scopeType, int scopeFlags)
    : m_scopeType(scopeType),
      m_scopeFlags(scopeFlags)
{
}

Scope::Scope(const Scope& other)
    : m_identifierTable(other.m_identifierTable),
      m_scopeType(other.m_scopeType),
      m_scopeFlags(other.m_scopeFlags),
      m_returnTypes(other.m_returnTypes),
      m_closureCaptures(other.m_closureCaptures),
      m_genericInstanceCache(other.m_genericInstanceCache)
{
}

} // namespace hyperion::compiler
