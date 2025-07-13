/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <analyzer/Module.hpp>

#include <core/functional/Proc.hpp>

namespace hyperion {
namespace buildtool {

Module::Module(const FilePath& path)
    : m_path(path)
{
}

Result Module::AddHypClassDefinition(HypClassDefinition&& hypClassDefinition)
{
    Mutex::Guard guard(m_mutex);

    auto it = m_hypClasses.Find(hypClassDefinition.name);

    if (it != m_hypClasses.End())
    {
        return HYP_MAKE_ERROR(Error, "HypClassDefinition already exists");
    }

    m_hypClasses.Insert(hypClassDefinition.name, std::move(hypClassDefinition));

    return {};
}

const HypClassDefinition* Module::FindHypClassDefinition(UTF8StringView className) const
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_hypClasses.Find(className);

    if (it == m_hypClasses.End())
    {
        return nullptr;
    }

    return &it->second;
}

bool Module::HasBaseClass(const HypClassDefinition& hypClassDefinition, UTF8StringView baseClassName) const
{
    Mutex::Guard guard(m_mutex);

    auto findDefinition = [this](UTF8StringView name) -> const HypClassDefinition*
    {
        auto it = m_hypClasses.Find(name);

        if (it != m_hypClasses.End())
        {
            return &it->second;
        }

        return nullptr;
    };

    Proc<bool(const HypClassDefinition&, UTF8StringView)> performCheck;

    performCheck = [&performCheck, &findDefinition](const HypClassDefinition& hypClassDefinition, UTF8StringView baseClassName) -> bool
    {
        auto it = hypClassDefinition.baseClassNames.FindAs(baseClassName);

        if (it != hypClassDefinition.baseClassNames.End())
        {
            return true;
        }

        for (const String& baseClass : hypClassDefinition.baseClassNames)
        {
            const HypClassDefinition* baseClassDefinition = findDefinition(baseClass);

            if (baseClassDefinition && performCheck(*baseClassDefinition, baseClassName))
            {
                return true;
            }
        }

        return false;
    };

    return performCheck(hypClassDefinition, baseClassName);
}

} // namespace buildtool
} // namespace hyperion