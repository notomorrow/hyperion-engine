#include <script/compiler/Module.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion::compiler {

Module::Module(
    const String& name,
    const SourceLocation& location)
    : m_name(name),
      m_location(location),
      m_treeLink(nullptr)
{
}

FlatSet<String> Module::GenerateAllScanPaths() const
{
    FlatSet<String> allScanPaths = m_scanPaths;

    TreeNode<Module*>* top = m_treeLink;

    while (top != nullptr)
    {
        Assert(top->Get() != nullptr);

        for (const auto& otherScanPath : top->Get()->GetScanPaths())
        {
            allScanPaths.Insert(otherScanPath);
        }

        top = top->m_parent;
    }

    return allScanPaths;
}

String Module::GenerateFullModuleName() const
{
    TreeNode<Module*>* top = m_treeLink;

    if (top != nullptr)
    {
        Array<String> parts;
        SizeType size = 0;

        do
        {
            Assert(top->Get() != nullptr);

            parts.PushBack(top->Get()->GetName());
            size += top->Get()->GetName().Size();

            top = top->m_parent;
        }
        while (top != nullptr);

        String res;
        res.Reserve(size + (2 * parts.Size()));

        for (int i = int(parts.Size()) - 1; i >= 0; i--)
        {
            res += parts[i];

            if (i != 0)
            {
                res += "::";
            }
        }

        return res;
    }

    return m_name;
}

bool Module::IsInGlobalScope() const
{
    return m_scopes.TopNode()->m_parent == nullptr;
}

bool Module::IsInScopeOfType(ScopeType scopeType) const
{
    const TreeNode<Scope>* top = m_scopes.TopNode();

    while (top != nullptr)
    {
        if (top->Get().GetScopeType() == scopeType)
        {
            return true;
        }

        top = top->m_parent;
    }

    return false;
}

bool Module::IsInScopeOfType(ScopeType scopeType, uint32 scopeFlags) const
{
    const TreeNode<Scope>* top = m_scopes.TopNode();

    while (top != nullptr)
    {
        if (top->Get().GetScopeType() == scopeType && bool(uint32(top->Get().GetScopeFlags()) & scopeFlags))
        {
            return true;
        }

        top = top->m_parent;
    }

    return false;
}

Module* Module::LookupNestedModule(const String& name)
{
    Assert(m_treeLink != nullptr);

    // search siblings of the current module,
    // rather than global lookup.
    for (auto* sibling : m_treeLink->m_siblings)
    {
        Assert(sibling != nullptr);
        Assert(sibling->Get() != nullptr);

        if (sibling->Get()->GetName() == name)
        {
            return sibling->Get();
        }
    }

    return nullptr;
}

Array<Module*> Module::CollectNestedModules() const
{
    Assert(m_treeLink != nullptr);

    Array<Module*> nestedModules;

    // search siblings of the current module,
    // rather than global lookup.
    for (auto* sibling : m_treeLink->m_siblings)
    {
        Assert(sibling != nullptr);
        Assert(sibling->Get() != nullptr);

        nestedModules.PushBack(sibling->Get());
    }

    return nestedModules;
}

RC<Identifier> Module::LookUpIdentifier(const String& name, bool thisScopeOnly, bool outsideModules)
{
    TreeNode<Scope>* top = m_scopes.TopNode();

    while (top != nullptr)
    {
        if (RC<Identifier> result = top->Get().GetIdentifierTable().LookUpIdentifier(name))
        {
            // a result was found
            return result;
        }

        if (thisScopeOnly)
        {
            return nullptr;
        }

        top = top->m_parent;
    }

    if (outsideModules)
    {
        if (m_treeLink != nullptr && m_treeLink->m_parent != nullptr)
        {
            if (Module* other = m_treeLink->m_parent->Get())
            {
                if (other->GetLocation().GetFileName() == m_location.GetFileName())
                {
                    return other->LookUpIdentifier(name, false);
                }
                else
                {
                    // we are outside of file scope, so loop until root/global module found
                    const TreeNode<Module*>* modLink = m_treeLink->m_parent;

                    while (modLink->m_parent != nullptr)
                    {
                        modLink = modLink->m_parent;
                    }

                    Assert(modLink->Get() != nullptr);
                    Assert(modLink->Get()->GetName() == Config::globalModuleName);

                    return modLink->Get()->LookUpIdentifier(name, false);
                }
            }
        }
    }

    return nullptr;
}

RC<Identifier> Module::LookUpIdentifierDepth(const String& name, int depthLevel)
{
    TreeNode<Scope>* top = m_scopes.TopNode();

    for (int i = 0; top != nullptr && i < depthLevel; i++)
    {
        if (RC<Identifier> result = top->Get().GetIdentifierTable().LookUpIdentifier(name))
        {
            return result;
        }

        top = top->m_parent;
    }

    return nullptr;
}

SymbolTypePtr_t Module::LookupSymbolType(const String& name)
{
    return PerformLookup<SymbolTypePtr_t>(
        [&name](TreeNode<Scope>* top)
        {
            return top->Get().GetIdentifierTable().LookupSymbolType(name);
        },
        [&name](Module* mod)
        {
            return mod->LookupSymbolType(name);
        });
}
Variant<RC<Identifier>, SymbolTypePtr_t> Module::LookUpIdentifierOrSymbolType(const String& name)
{
    return PerformLookup<Variant<RC<Identifier>, SymbolTypePtr_t>>(
        [&name](TreeNode<Scope>* top) -> Variant<RC<Identifier>, SymbolTypePtr_t>
        {
            if (SymbolTypePtr_t symbolType = top->Get().GetIdentifierTable().LookupSymbolType(name))
            {
                return Variant<RC<Identifier>, SymbolTypePtr_t>(symbolType);
            }

            if (RC<Identifier> result = top->Get().GetIdentifierTable().LookUpIdentifier(name))
            {
                return Variant<RC<Identifier>, SymbolTypePtr_t>(result);
            }

            return Variant<RC<Identifier>, SymbolTypePtr_t>();
        },
        [&name](Module* mod) -> Variant<RC<Identifier>, SymbolTypePtr_t>
        {
            return mod->LookUpIdentifierOrSymbolType(name);
        });
}

Optional<GenericInstanceCache::CachedObject> Module::LookupGenericInstance(const GenericInstanceCache::Key& key)
{
    return PerformLookup<Optional<GenericInstanceCache::CachedObject>>(
        [&key](TreeNode<Scope>* top)
        {
            return top->Get().GetGenericInstanceCache().Lookup(key);
        },
        [&key](Module* mod)
        {
            return mod->LookupGenericInstance(key);
        });
}

} // namespace hyperion::compiler
