#pragma once

namespace hyperion {
namespace vm {
class VM;
}

class APIInstance;

namespace compiler {
class CompilationUnit;

class Meta
{
public:
    static void BuildMetaLibrary(hyperion::vm::VM& vm,
        CompilationUnit& compilationUnit,
        hyperion::APIInstance& api);
};
} // namespace compiler
} // namespace hyperion

