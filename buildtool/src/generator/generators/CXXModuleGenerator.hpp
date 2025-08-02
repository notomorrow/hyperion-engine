/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_CXX_MODULE_GENERATOR_HPP
#define HYPERION_BUILDTOOL_CXX_MODULE_GENERATOR_HPP

#include <generator/Generator.hpp>

namespace hyperion {
namespace buildtool {

class CXXModuleGenerator : public GeneratorBase
{
public:
    virtual ~CXXModuleGenerator() override = default;

    HYP_FORCE_INLINE Result Generate(const Analyzer& analyzer, const Module& mod) const
    {
        return GeneratorBase::Generate(analyzer, mod);
    }
    virtual Result Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const override;
    virtual FilePath GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const override;

protected:
};

} // namespace buildtool
} // namespace hyperion

#endif
