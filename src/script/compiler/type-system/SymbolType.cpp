#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>

#include <core/containers/FlatSet.hpp>

#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

namespace hyperion::compiler {

SymbolType::SymbolType(
    const String& name,
    SymbolTypeClass typeClass,
    const SymbolTypePtr_t& base)
    : m_name(name),
      m_typeClass(typeClass),
      m_defaultValue(nullptr),
      m_base(base),
      m_id(-1),
      m_flags(SYMBOL_TYPE_FLAGS_NONE)
{
}

SymbolType::SymbolType(
    const String& name,
    SymbolTypeClass typeClass,
    const SymbolTypePtr_t& base,
    const RC<AstExpression>& defaultValue,
    const Array<SymbolTypeMember>& members)
    : m_name(name),
      m_typeClass(typeClass),
      m_defaultValue(defaultValue),
      m_members(members),
      m_base(base),
      m_id(-1),
      m_flags(SYMBOL_TYPE_FLAGS_NONE)
{
}

bool SymbolType::TypeEqual(const SymbolType& other) const
{
    if (std::addressof(other) == this)
    {
        return true;
    }

    return GetHashCode() == other.GetHashCode();

    if (m_name != other.m_name)
    {
        return false;
    }

    if (m_typeClass != other.m_typeClass)
    {
        return false;
    }

    switch (m_typeClass)
    {
    case TYPE_BUILTIN:
        return true;
    case TYPE_ALIAS:
        if (SymbolTypePtr_t sp = m_aliasInfo.m_aliasee.lock())
        {
            return *sp == other;
        }

        return false;

    case TYPE_GENERIC:
        if (m_genericInfo.m_numParameters != other.m_genericInfo.m_numParameters)
        {
            return false;
        }

        return true;

    case TYPE_GENERIC_PARAMETER:
        return true;

    case TYPE_GENERIC_INSTANCE:
    {
        if (m_genericInstanceInfo.m_genericArgs.Size() != other.m_genericInstanceInfo.m_genericArgs.Size())
        {
            return false;
        }

        SymbolTypePtr_t base = m_base;
        Assert(base != nullptr);

        // check for compatibility between instances
        SymbolTypePtr_t otherBase = other.GetBaseType();
        Assert(otherBase != nullptr);

        if (!base->TypeEqual(*otherBase))
        {
            return false;
        }

        // check all params
        if (m_genericInstanceInfo.m_genericArgs.Size() != other.m_genericInstanceInfo.m_genericArgs.Size())
        {
            return false;
        }

        // check each substituted parameter
        for (SizeType i = 0; i < m_genericInstanceInfo.m_genericArgs.Size(); i++)
        {
            const SymbolTypePtr_t& instanceArgType = m_genericInstanceInfo.m_genericArgs[i].m_type;
            const SymbolTypePtr_t& otherArgType = other.m_genericInstanceInfo.m_genericArgs[i].m_type;

            Assert(instanceArgType != nullptr);
            Assert(otherArgType != nullptr);

            // DebugLog(LogType::Debug, "Compare generic instance args: %s == %s %llu %llu\n",
            //     instanceArgType->ToString(true).Data(),
            //     otherArgType->ToString(true).Data(),
            //     instanceArgType->m_genericInstanceInfo.m_genericArgs.Size(),
            //     otherArgType->m_genericInstanceInfo.m_genericArgs.Size());

            if (!instanceArgType->TypeEqual(*otherArgType))
            {
                return false; // have to do this for now to prevent infinte recursion
            }
        }

        break;
    }
    default:
        break;
    }

    // early out if members are not equal
    if (m_members.Size() != other.m_members.Size())
    {
        return false;
    }

    for (const SymbolTypeMember& leftMember : m_members)
    {
        const SymbolTypePtr_t& leftMemberType = leftMember.type;
        Assert(leftMemberType != nullptr);

        SymbolTypeMember rightMember;

        if (!other.FindMember(leftMember.name, rightMember))
        {
            // DebugLog(LogType::Debug,
            //     "SymbolType::TypeEqual: member '%s' not found in other type '%s'\n",
            //     leftMember.name.Data(),
            //     other.m_name.Data());

            return false;
        }

        Assert(rightMember.type != nullptr);

        // DebugLog(LogType::Debug,
        //     "SymbolType::TypeEqual: check member '%s' type '%s' == '%s'\n",
        //     leftMember.name.Data(),
        //     leftMemberType->ToString(true).Data(),
        //     rightMember.type->ToString(true).Data());

        if (!rightMember.type->TypeEqual(*leftMemberType))
        {
            // DebugLog(LogType::Debug,
            //     "SymbolType::TypeEqual: member '%s' type '%s' != '%s'\n",
            //     leftMember.name.Data(),
            //     leftMemberType->ToString(true).Data(),
            //     rightMember.type->ToString(true).Data());

            return false;
        }
    }

    return true;
}

bool SymbolType::TypeCompatible(
    const SymbolType& right,
    bool strictNumbers) const
{
    if (TypeEqual(*BuiltinTypes::UNDEFINED) || right.TypeEqual(*BuiltinTypes::UNDEFINED))
    {
        return false;
    }

    if (TypeEqual(right))
    {
        return true;
    }

    if (IsAnyType() || right.IsAnyType())
    {
        return true;
    }

    if (IsPlaceholderType() || right.IsPlaceholderType())
    {
        return true;
    }

    // if (IsProxyClass()) {
    //     // TODO:
    //     // have proxy class declare which class it is a proxy for,
    //     // then check that the types match?
    //     return true;
    // }

    if (IsNullType())
    {
        return right.IsNullableType();
    }

    if (right.IsNullType())
    {
        return IsNullableType();
    }

    if (right.IsGenericParameter())
    {
        // no substitution yet, compatible
        return true;
    }

    switch (m_typeClass)
    {
    case TYPE_ALIAS:
    {
        SymbolTypePtr_t sp = m_aliasInfo.m_aliasee.lock();
        Assert(sp != nullptr);

        return sp->TypeCompatible(right, strictNumbers);
    }
    case TYPE_GENERIC:
    {
        if (right.m_typeClass == TYPE_GENERIC || right.m_typeClass == TYPE_GENERIC_INSTANCE)
        {
            if (auto rightBase = right.GetBaseType())
            {
                if (TypeCompatible(*rightBase, strictNumbers))
                {
                    return true;
                }
            }
        }

        return false;
    }
    case TYPE_GENERIC_INSTANCE:
    {
        SymbolTypePtr_t base = m_base;
        Assert(base != nullptr);

        if (right.m_typeClass == TYPE_GENERIC_INSTANCE)
        {
            // check for compatibility between instances
            SymbolTypePtr_t otherBase = right.GetBaseType();
            Assert(otherBase != nullptr);

            if (!base->TypeEqual(*otherBase))
            {
                return false;
            }

            // check all params
            if (m_genericInstanceInfo.m_genericArgs.Size() != right.m_genericInstanceInfo.m_genericArgs.Size())
            {
                return false;
            }

            // check each substituted parameter
            for (SizeType i = 0; i < m_genericInstanceInfo.m_genericArgs.Size(); i++)
            {
                const SymbolTypePtr_t& paramType = m_genericInstanceInfo.m_genericArgs[i].m_type;
                const SymbolTypePtr_t& otherParamType = right.m_genericInstanceInfo.m_genericArgs[i].m_type;

                Assert(paramType != nullptr);
                Assert(otherParamType != nullptr);

                if (!paramType->TypeEqual(*otherParamType))
                {
                    return false;
                }
            }

            return true;
        }
        else
        {
            return false;
        }

        break;
    }

    case TYPE_GENERIC_PARAMETER:
    {
        // uninstantiated generic parameters are compatible with anything
        return true;
    }

    default:
        if (!strictNumbers && IsNumber() && right.IsNumber())
        {
            return true;
        }

        return false;
    }

    return true;
}

const SymbolTypePtr_t SymbolType::FindMember(const String& name) const
{
    for (const SymbolTypeMember& member : m_members)
    {
        if (member.name == name)
        {
            return member.type;
        }
    }

    return nullptr;
}

bool SymbolType::FindMember(const String& name, SymbolTypeMember& out) const
{
    for (const SymbolTypeMember& member : m_members)
    {
        if (member.name == name)
        {
            out = member;
            return true;
        }
    }

    return false;
}

bool SymbolType::FindMember(const String& name, SymbolTypeMember& out, uint32& outIndex) const
{
    // get member index from name
    for (SizeType i = 0; i < m_members.Size(); i++)
    {
        const SymbolTypeMember& member = m_members[i];

        if (member.name == name)
        {
            // only set m_foundIndex if found in first level.
            // for members from base objects,
            // we load based on hash.
            outIndex = uint32(i);
            out = member;

            return true;
        }
    }

    return false;
}

bool SymbolType::FindMemberDeep(const String& name, SymbolTypeMember& out) const
{
    uint32 outIndex;
    uint32 outDepth;

    return FindMemberDeep(name, out, outIndex, outDepth);
}

bool SymbolType::FindMemberDeep(const String& name, SymbolTypeMember& out, uint32& outIndex) const
{
    uint32 outDepth;

    return FindMemberDeep(name, out, outIndex, outDepth);
}

bool SymbolType::FindMemberDeep(const String& name, SymbolTypeMember& out, uint32& outIndex, uint32& outDepth) const
{
    outDepth = 0;

    if (FindMember(name, out, outIndex))
    {
        return true;
    }

    outDepth++;

    SymbolTypePtr_t basePtr = GetBaseType();

    while (basePtr != nullptr)
    {
        if (basePtr->FindMember(name, out, outIndex))
        {
            return true;
        }

        basePtr = basePtr->GetBaseType();

        outDepth++;
    }

    return false;
}

const SymbolTypePtr_t SymbolType::FindPrototypeMember(const String& name) const
{
    if (SymbolTypePtr_t protoType = FindMember("$proto"))
    {
        if (protoType->IsAnyType() || protoType->IsPlaceholderType())
        {
            return BuiltinTypes::ANY;
        }

        return protoType->FindMember(name);
    }

    return nullptr;
}

bool SymbolType::FindPrototypeMember(const String& name, SymbolTypeMember& out) const
{
    if (SymbolTypePtr_t protoType = FindMember("$proto"))
    {
        return protoType->FindMember(name, out);
    }

    return false;
}

bool SymbolType::FindPrototypeMemberDeep(const String& name) const
{
    SymbolTypeMember out;

    return FindPrototypeMemberDeep(name, out);
}

bool SymbolType::FindPrototypeMemberDeep(const String& name, SymbolTypeMember& out) const
{
    if (FindPrototypeMember(name, out))
    {
        return true;
    }

    SymbolTypePtr_t basePtr = GetBaseType();

    while (basePtr != nullptr)
    {
        if (basePtr->FindPrototypeMember(name, out))
        {
            return true;
        }

        basePtr = basePtr->GetBaseType();
    }

    return false;
}

bool SymbolType::FindPrototypeMember(const String& name, SymbolTypeMember& out, uint32& outIndex) const
{
    bool found = false;

    // for instance members (do it last, so it can be overridden by instances)
    if (SymbolTypePtr_t protoType = FindMember("$proto"))
    {
        // get member index from name
        for (SizeType i = 0; i < protoType->GetMembers().Size(); i++)
        {
            const SymbolTypeMember& member = protoType->GetMembers()[i];

            if (member.name == name)
            {
                // only set m_foundIndex if found in first level.
                // for members from base objects,
                // we load based on hash.
                outIndex = uint32(i);
                out = member;

                found = true;

                break;
            }
        }
    }

    return found;
}

bool SymbolType::HasTrait(const SymbolTypeTrait& trait) const
{
    SymbolTypeMember member;

    // trait names are prefixed with '@'
    if (FindMember(trait.name, member))
    {
        return true;
    }

    return false;
}

bool SymbolType::HasTraitDeep(const SymbolTypeTrait& trait) const
{
    SymbolTypeMember member;

    if (FindMemberDeep(trait.name, member))
    {
        return true;
    }

    return false;
}

bool SymbolType::IsOrHasBase(const SymbolType& baseType) const
{
    return TypeEqual(baseType) || HasBase(baseType);
}

bool SymbolType::HasBase(const SymbolType& baseType) const
{
    if (SymbolTypePtr_t thisBase = GetBaseType())
    {
        if (thisBase->TypeEqual(baseType))
        {
            return true;
        }
        else
        {
            return thisBase->HasBase(baseType);
        }
    }

    return false;
}

SymbolTypePtr_t SymbolType::GetUnaliased()
{
    if (m_typeClass == TYPE_ALIAS)
    {
        if (const SymbolTypePtr_t aliasee = m_aliasInfo.m_aliasee.lock())
        {
            if (aliasee.get() == this)
            {
                return aliasee;
            }

            return aliasee->GetUnaliased();
        }
    }

    return shared_from_this();
}

bool SymbolType::IsNumber() const
{
    return IsOrHasBase(*BuiltinTypes::INT)
        || IsOrHasBase(*BuiltinTypes::UNSIGNED_INT)
        || IsOrHasBase(*BuiltinTypes::FLOAT);
}

bool SymbolType::IsIntegral() const
{
    return IsOrHasBase(*BuiltinTypes::INT)
        || IsOrHasBase(*BuiltinTypes::UNSIGNED_INT);
}

bool SymbolType::IsSignedIntegral() const
{
    return IsOrHasBase(*BuiltinTypes::INT);
}

bool SymbolType::IsUnsignedIntegral() const
{
    return IsOrHasBase(*BuiltinTypes::UNSIGNED_INT);
}

bool SymbolType::IsFloat() const
{
    return IsOrHasBase(*BuiltinTypes::FLOAT);
}

bool SymbolType::IsBoolean() const
{
    return IsOrHasBase(*BuiltinTypes::BOOLEAN);
}

bool SymbolType::IsClass() const
{
    return IsOrHasBase(*BuiltinTypes::CLASS_TYPE);
}

bool SymbolType::IsObject() const
{
    return IsOrHasBase(*BuiltinTypes::OBJECT);
}

bool SymbolType::IsAnyType() const
{
    return IsOrHasBase(*BuiltinTypes::ANY);
}

bool SymbolType::IsPlaceholderType() const
{
    return IsOrHasBase(*BuiltinTypes::PLACEHOLDER);
}

bool SymbolType::IsNullType() const
{
    return IsOrHasBase(*BuiltinTypes::NULL_TYPE);
}

bool SymbolType::IsNullableType() const
{
    return IsOrHasBase(*BuiltinTypes::OBJECT);
}

bool SymbolType::IsVarArgsType() const
{
    return HasTraitDeep(BuiltinTypeTraits::variadic);
}

bool SymbolType::IsGenericParameter() const
{
    return m_typeClass == TYPE_GENERIC_PARAMETER;
}

bool SymbolType::IsGenericExpressionType() const
{
    return GetBaseType() == BuiltinTypes::GENERIC_VARIABLE_TYPE;
}

bool SymbolType::IsGenericInstanceType() const
{
    return m_typeClass == TYPE_GENERIC_INSTANCE;
}

bool SymbolType::IsGenericBaseType() const
{
    return m_typeClass == TYPE_GENERIC;
}

bool SymbolType::IsPrimitive() const
{
    return IsOrHasBase(*BuiltinTypes::PRIMITIVE_TYPE);
}

bool SymbolType::IsEnumType() const
{
    return IsOrHasBase(*BuiltinTypes::ENUM_TYPE);
}

SymbolTypePtr_t SymbolType::Alias(
    const String& name,
    const AliasTypeInfo& info)
{
    if (auto sp = info.m_aliasee.lock())
    {
        SymbolTypePtr_t res(new SymbolType(
            name,
            TYPE_ALIAS,
            nullptr));

        res->m_aliasInfo = info;
        res->SetId(sp->GetId());

        return res;
    }

    return nullptr;
}

SymbolTypePtr_t SymbolType::Primitive(
    const String& name,
    const RC<AstExpression>& defaultValue)
{
    return SymbolTypePtr_t(new SymbolType(
        name,
        TYPE_BUILTIN,
        BuiltinTypes::PRIMITIVE_TYPE,
        defaultValue,
        {}));
}

SymbolTypePtr_t SymbolType::Primitive(
    const String& name,
    const RC<AstExpression>& defaultValue,
    const SymbolTypePtr_t& base)
{
    return SymbolTypePtr_t(new SymbolType(
        name,
        TYPE_BUILTIN,
        base,
        defaultValue,
        {}));
}

SymbolTypePtr_t SymbolType::Object(
    const String& name,
    const Array<SymbolTypeMember>& members)
{
    SymbolTypePtr_t symbolType(new SymbolType(
        name,
        TYPE_USER_DEFINED,
        BuiltinTypes::CLASS_TYPE,
        nullptr,
        members));

    // symbolType->SetDefaultValue(RC<AstObject>(
    //     new AstObject(symbolType, SourceLocation::eof)
    // ));

    return symbolType;
}

SymbolTypePtr_t SymbolType::Object(const String& name,
    const Array<SymbolTypeMember>& members,
    const SymbolTypePtr_t& base)
{
    SymbolTypePtr_t symbolType(new SymbolType(
        name,
        TYPE_USER_DEFINED,
        base,
        nullptr,
        members));

    // symbolType->SetDefaultValue(RC<AstObject>(
    //     new AstObject(symbolType, SourceLocation::eof)
    // ));

    return symbolType;
}

SymbolTypePtr_t SymbolType::Generic(const String& name,
    const Array<SymbolTypeMember>& members,
    const GenericTypeInfo& info,
    const SymbolTypePtr_t& base)
{
    SymbolTypePtr_t res(new SymbolType(
        name,
        TYPE_GENERIC,
        base,
        nullptr,
        members));

    res->m_genericInfo = info;

    return res;
}

SymbolTypePtr_t SymbolType::Generic(const String& name,
    const RC<AstExpression>& defaultValue,
    const Array<SymbolTypeMember>& members,
    const GenericTypeInfo& info,
    const SymbolTypePtr_t& base)
{
    SymbolTypePtr_t res(new SymbolType(
        name,
        TYPE_GENERIC,
        base,
        defaultValue,
        members));

    res->m_genericInfo = info;

    return res;
}

String SymbolType::ToString(bool includeParameterNames) const
{
    String res = m_name;

    if (SymbolTypePtr_t sp = m_aliasInfo.m_aliasee.lock())
    {
        res += " (aka " + sp->ToString() + ")";
    }

    switch (m_typeClass)
    {
    case TYPE_ALIAS:
    case TYPE_BUILTIN: // fallthrough
    case TYPE_USER_DEFINED:
    case TYPE_GENERIC_PARAMETER:
    case TYPE_GENERIC:
        break;
    case TYPE_GENERIC_INSTANCE:
    {
        res = m_name;

        const GenericInstanceTypeInfo& info = m_genericInstanceInfo;

        if (info.m_genericArgs.Any())
        {
            if (IsVarArgsType())
            {
                const SymbolTypePtr_t& heldType = info.m_genericArgs.Front().m_type;
                Assert(heldType != nullptr);

                return heldType->ToString() + "...";
            }
            else
            {
                res += "<";

                bool hasReturnType = false;
                String returnTypeName;

                for (SizeType i = 0; i < info.m_genericArgs.Size(); i++)
                {
                    const String& genericArgName = info.m_genericArgs[i].m_name;
                    const SymbolTypePtr_t& genericArgType = info.m_genericArgs[i].m_type;

                    Assert(genericArgType != nullptr);

                    if (genericArgName == "@return")
                    {
                        hasReturnType = true;
                        returnTypeName = genericArgType->ToString();
                    }
                    else
                    {
                        if (info.m_genericArgs[i].m_isConst)
                        {
                            res += "const ";
                        }

                        if (info.m_genericArgs[i].m_isRef)
                        {
                            res += "ref ";
                        }

                        if (includeParameterNames && genericArgName.Any())
                        {
                            res += genericArgName;
                            res += ": ";
                        }

                        res += genericArgType->ToString(includeParameterNames);

                        if (i != info.m_genericArgs.Size() - 1)
                        {
                            res += ", ";
                        }
                    }
                }

                res += ">";

                if (hasReturnType)
                {
                    res += " -> " + returnTypeName;
                }
            }
        }

        break;
    }
    default:
        break;
    }

    return res;
}

SymbolTypePtr_t SymbolType::GenericInstance(
    const SymbolTypePtr_t& base,
    const GenericInstanceTypeInfo& info,
    const Array<SymbolTypeMember>& members)
{
    Assert(info.m_genericArgs.Size() >= 1,
        "Generic Instances must have at least 1 argument (@return is the first argument and must always be present)");

    Assert(base != nullptr);
    // Assert(base->GetTypeClass() == TYPE_GENERIC_INSTANCE || base->GetTypeClass() == TYPE_GENERIC);

    const String name = base->GetName();

    Array<SymbolTypeMember> allMembers;
    allMembers.Reserve(base->GetMembers().Size() + members.Size());

    for (const SymbolTypeMember& member : members)
    {
        allMembers.PushBack(SymbolTypeMember {
            member.name,
            member.type,
            CloneAstNode(member.expr) });
    }

    SymbolTypePtr_t currentBase = base;

    while (currentBase != nullptr)
    {
        for (const SymbolTypeMember& member : currentBase->GetMembers())
        {
            const auto overridenMemberIt = allMembers.FindIf([&member](const auto& otherMember)
                {
                    return otherMember.name == member.name;
                });

            if (overridenMemberIt != allMembers.End())
            {
                // if member is overriden, skip it
                continue;
            }

            // push copy (clone assignment value)
            allMembers.PushBack(SymbolTypeMember {
                member.name,
                member.type,
                CloneAstNode(member.expr) });
        }

        currentBase = currentBase->GetBaseType();
    }

    SymbolTypePtr_t res(new SymbolType(
        name,
        TYPE_GENERIC_INSTANCE,
        base,
        nullptr,
        allMembers));

    auto defaultValue = base->GetDefaultValue();

    res->SetDefaultValue(defaultValue);
    res->SetFlags(base->GetFlags());
    res->m_genericInstanceInfo = info;

    return res;
}

SymbolTypePtr_t SymbolType::GenericInstance(
    const SymbolTypePtr_t& base,
    const GenericInstanceTypeInfo& info)
{
    return GenericInstance(base, info, {});
}

SymbolTypePtr_t SymbolType::GenericParameter(
    const String& name,
    const SymbolTypePtr_t& base)
{
    SymbolTypePtr_t res(new SymbolType(
        name,
        TYPE_GENERIC_PARAMETER,
        base));

    return res;
}

SymbolTypePtr_t SymbolType::Extend(
    const String& name,
    const SymbolTypePtr_t& base,
    const Array<SymbolTypeMember>& members)
{
    Assert(base != nullptr);

    SymbolTypePtr_t symbolType(new SymbolType(
        name,
        base->GetTypeClass() == TYPE_BUILTIN
            ? TYPE_USER_DEFINED
            : base->GetTypeClass(),
        base,
        base->GetDefaultValue(),
        members));

    // symbolType->SetDefaultValue(RC<AstObject>(
    //     new AstObject(symbolType, SourceLocation::eof)
    // ));

    symbolType->m_genericInfo = base->m_genericInfo;
    symbolType->m_genericInstanceInfo = base->m_genericInstanceInfo;
    symbolType->m_genericParamInfo = base->m_genericParamInfo;
    symbolType->m_functionInfo = base->m_functionInfo;

    return symbolType;
}

SymbolTypePtr_t SymbolType::TypePromotion(
    const SymbolTypePtr_t& lptr,
    const SymbolTypePtr_t& rptr)
{
    if (lptr == nullptr || rptr == nullptr)
    {
        return nullptr;
    }

    // compare pointer values
    if (lptr == rptr || lptr->TypeEqual(*rptr))
    {
        return lptr;
    }

    if (lptr->TypeEqual(*BuiltinTypes::UNDEFINED) || rptr->TypeEqual(*BuiltinTypes::UNDEFINED))
    {
        return BuiltinTypes::UNDEFINED;
    }
    else if (lptr->IsAnyType() || rptr->IsAnyType())
    {
        // Any + T = Any
        // T + Any = Any
        return BuiltinTypes::ANY;
    }
    else if (lptr->IsGenericParameter() || rptr->IsGenericParameter())
    {
        /* @TODO: Might be useful to use the base type of the generic. */
        return BuiltinTypes::PLACEHOLDER;
    }
    else if (lptr->IsPlaceholderType() || rptr->IsPlaceholderType())
    {
        return BuiltinTypes::PLACEHOLDER;
    }
    else if (lptr->IsNumber() && rptr->IsNumber())
    {
        if (lptr->IsFloat() || rptr->IsFloat())
        {
            return BuiltinTypes::FLOAT;
        }
        else if (lptr->IsUnsignedIntegral() || rptr->IsUnsignedIntegral())
        {
            return BuiltinTypes::UNSIGNED_INT;
        }
        else
        {
            return BuiltinTypes::INT;
        }
    }

    // @TODO Check for common base

    return BuiltinTypes::UNDEFINED;
}

SymbolTypePtr_t SymbolType::GenericPromotion(
    const SymbolTypePtr_t& lptr,
    const SymbolTypePtr_t& rptr)
{
    Assert(lptr != nullptr);
    Assert(rptr != nullptr);

    switch (lptr->GetTypeClass())
    {
    case TYPE_GENERIC:
        switch (rptr->GetTypeClass())
        {
        case TYPE_GENERIC_INSTANCE:
        {
            auto rightBase = rptr->GetBaseType();

            while (rightBase != nullptr)
            {
                if (lptr->TypeEqual(*rightBase))
                {
                    // left-hand side is the base of the right hand side,
                    // so upgrade left to the derived type.
                    return rptr;
                }
                rightBase = rightBase->GetBaseType();
            }
        }
            // fallthrough
        default:
            break;
        }

        break;

    case TYPE_GENERIC_INSTANCE:
    {
        break;
    }
    }

    // no promotion
    return lptr;
}

SymbolTypePtr_t SymbolType::SubstituteGenericParams(
    const SymbolTypePtr_t& lptr,
    const SymbolTypePtr_t& placeholder,
    const SymbolTypePtr_t& substitute)
{
    Assert(lptr != nullptr);
    Assert(placeholder != nullptr);
    Assert(substitute != nullptr);

    if (lptr->TypeEqual(*placeholder))
    {
        return substitute;
    }

    switch (lptr->GetTypeClass())
    {
    case TYPE_GENERIC_INSTANCE:
    {
        SymbolTypePtr_t baseType = lptr->GetBaseType();
        Assert(baseType != nullptr);

        Array<GenericInstanceTypeInfo::Arg> newGenericTypes;

        for (const GenericInstanceTypeInfo::Arg& arg : lptr->GetGenericInstanceInfo().m_genericArgs)
        {
            const SymbolTypePtr_t& argType = arg.m_type;
            Assert(argType != nullptr);

            GenericInstanceTypeInfo::Arg newArg;
            newArg.m_name = arg.m_name;
            newArg.m_defaultValue = arg.m_defaultValue;

            // perform substitution
            SymbolTypePtr_t argTypeSubstituted = SubstituteGenericParams(
                argType,
                placeholder,
                substitute);

            Assert(argTypeSubstituted != nullptr);
            newArg.m_type = argTypeSubstituted;

            newGenericTypes.PushBack(newArg);
        }

        return SymbolType::GenericInstance(
            baseType,
            GenericInstanceTypeInfo {
                newGenericTypes });
    }
    }

    return lptr;
}

HashCode SymbolType::GetHashCodeWithDuplicateRemoval(FlatSet<String>& duplicateNames) const
{
    if (duplicateNames.Contains(m_name))
    {
        return HashCode();
    }

    duplicateNames.Insert(m_name);

    HashCode hc;
    hc.Add(m_name);
    hc.Add(m_typeClass);
    hc.Add(m_base ? m_base->GetHashCodeWithDuplicateRemoval(duplicateNames) : HashCode());
    hc.Add(m_flags);

    switch (m_typeClass)
    {
    case TYPE_BUILTIN:
        break;
    case TYPE_ALIAS:
        if (auto aliasee = m_aliasInfo.m_aliasee.lock())
        {
            hc.Add(aliasee->GetHashCodeWithDuplicateRemoval(duplicateNames));
        }
        else
        {
            hc.Add(HashCode());
        }

        break;
    case TYPE_GENERIC:
        hc.Add(m_genericInfo.m_numParameters);

        for (const auto& param : m_genericInfo.m_params)
        {
            hc.Add(param ? param->GetHashCodeWithDuplicateRemoval(duplicateNames) : HashCode());
        }

        break;
    case TYPE_GENERIC_INSTANCE:
        for (const auto& arg : m_genericInstanceInfo.m_genericArgs)
        {
            hc.Add(arg.m_name);
            hc.Add(arg.m_type ? arg.m_type->GetHashCodeWithDuplicateRemoval(duplicateNames) : HashCode());
        }

        break;
    case TYPE_GENERIC_PARAMETER:
        break;
    case TYPE_USER_DEFINED:
        break;
    }

    for (const SymbolTypeMember& member : m_members)
    {
        hc.Add(member.name);

        if (!member.type)
        {
            continue;
        }

        hc.Add(member.type->GetHashCodeWithDuplicateRemoval(duplicateNames));
    }

    return hc;
}

} // namespace hyperion::compiler
