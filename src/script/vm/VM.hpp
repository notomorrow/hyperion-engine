#ifndef VM_HPP
#define VM_HPP

#include <script/vm/BytecodeStream.hpp>
#include <script/vm/VMState.hpp>
#include <script/vm/StackTrace.hpp>

#include <Types.hpp>

#include <array>
#include <limits>
#include <cstdint>
#include <cstdio>

#define MAIN_THREAD m_threads[0]

#define MATCH_TYPES(left_type, right_type) \
    ((left_type) < (right_type)) ? (right_type) : (left_type)

namespace hyperion {

class APIInstance;

namespace vm {

class VM
{
public:
    VM(APIInstance &api_instance);
    VM(const VM &other) = delete;
    VM &operator=(const VM &other) = delete;
    VM(VM &&other) noexcept = delete;
    VM &operator=(VM &&other) noexcept = delete;
    ~VM();

    void PushNativeFunctionPtr(NativeFunctionPtr_t ptr);

    VMState &GetState()
        { return m_state; }

    const VMState &GetState() const
        { return m_state; }
    
    void Invoke(
        InstructionHandler *handler,
        const Value &value,
        UInt8 nargs
    );

    void InvokeNow(
        BytecodeStream *bs,
        const Value &value,
        UInt8 nargs
    );

    void Execute(BytecodeStream *bs);

private:
    bool HandleException(InstructionHandler *handler);
    void CreateStackTrace(ExecutionThread *thread, StackTrace *out);

    APIInstance     &m_api_instance;
    VMState         m_state;
};

} // namespace vm
} // namespace hyperion

#endif
