/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifndef HYPERION_CODEGEN_CSHARP_MODULE_GENERATOR_HPP
#define HYPERION_CODEGEN_CSHARP_MODULE_GENERATOR_HPP

#include <generator/Generator.hpp>

#include <Core/Containers/Set.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Threading/Mutex.hpp>

namespace Hyperion {
namespace CodeGen {

class CSharpModuleGenerator : public GeneratorBase
{
public:
    virtual ~CSharpModuleGenerator() override = default;

    HYP_FORCE_INLINE Result Generate(const Analyzer& analyzer, const Module& mod, bool* outWasWritten = nullptr) const
    {
        return GeneratorBase::Generate(analyzer, mod, outWasWritten);
    }

    virtual Result Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const override;
    virtual FilePath GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const override;

    HYP_FORCE_INLINE const Set<String>& GetWrittenClassNames() const
    {
        return m_writtenClassNames;
    }

protected:
    mutable Set<String> m_writtenClassNames;
    mutable Mutex m_writtenClassNamesMutex;
};

} // namespace CodeGen
} // namespace Hyperion

#endif
