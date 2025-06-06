/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_MODULE_HPP
#define HYPERION_BUILDTOOL_MODULE_HPP

#include <analyzer/Definitions.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/Array.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/utilities/Result.hpp>

#include <core/threading/Mutex.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace buildtool {

class Module
{
public:
    Module(const FilePath& path);
    ~Module() = default;

    HYP_FORCE_INLINE const FilePath& GetPath() const
    {
        return m_path;
    }

    HYP_FORCE_INLINE HashMap<String, HypClassDefinition>& GetHypClasses()
    {
        return m_hyp_classes;
    }

    HYP_FORCE_INLINE const HashMap<String, HypClassDefinition>& GetHypClasses() const
    {
        return m_hyp_classes;
    }

    Result AddHypClassDefinition(HypClassDefinition&& hyp_class_definition);

    const HypClassDefinition* FindHypClassDefinition(UTF8StringView class_name) const;

private:
    FilePath m_path;
    HashMap<String, HypClassDefinition> m_hyp_classes;
    mutable Mutex m_mutex;
};

} // namespace buildtool
} // namespace hyperion

#endif
