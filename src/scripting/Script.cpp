#include <scripting/Script.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

Script::Script()
    : Script(ScriptDesc {})
{
}

Script::Script(const ScriptDesc& desc)
    : m_desc(desc)
{
    AssertThrowMsg(m_desc.path.Size() + 1 <= scriptMaxPathLength, "Invalid script path: must be <= %llu characters", scriptMaxPathLength - 1);

    Memory::StrCpy(m_managedScript.path, m_desc.path.Data(), MathUtil::Min(m_desc.path.Size() + 1, scriptMaxPathLength));
    Memory::MemSet(m_managedScript.assemblyPath, '\0', scriptMaxPathLength);
    m_managedScript.state = uint32(CompiledScriptState::UNINITIALIZED);
}

Script::~Script() = default;

EnumFlags<CompiledScriptState> Script::GetState() const
{
    return EnumFlags<CompiledScriptState>(m_managedScript.state);
}

} // namespace hyperion