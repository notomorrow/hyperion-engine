/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifndef HYPERION_CODEGEN_ENGINE_TYPES_HPP
#define HYPERION_CODEGEN_ENGINE_TYPES_HPP

#include <Core/Containers/Set.hpp>
#include <Core/Containers/String.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/Result.hpp>

namespace Hyperion {
namespace CodeGen {

class Analyzer;

Result WriteEngineTypes(const Analyzer& analyzer, const Set<String>& csharpClassNames, const FilePath& filePath);
Result LoadEngineTypes(Analyzer& analyzer, const FilePath& filePath);

} // namespace CodeGen
} // namespace Hyperion

#endif
