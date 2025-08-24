#include <script/compiler/meta-scripting/Meta.hpp>
#include <script/ScriptApi.hpp>

#include <script/vm/InstructionHandler.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/Exception.hpp>

namespace hyperion::compiler {

void MetaDefine(sdk::Params params)
{
    HYP_SCRIPT_CHECK_ARGS(==, 4);

    // arguments should be:
    //   __meta_context object (UserData)
    //   identifier (String)
    //   value (Any)

    vm::Value* metaContextPtr = params.args[0];
    Assert(metaContextPtr != nullptr);
    Assert(metaContextPtr->m_type == vm::Value::USER_DATA);
}

void Meta::BuildMetaLibrary(vm::VM& vm,
    CompilationUnit& compilationUnit,
    APIInstance& api)
{
}

} // namespace hyperion::compiler
