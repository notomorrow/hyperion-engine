#pragma once

#include <core/utilities/TypeId.hpp>
#include <core/containers/String.hpp>
#include <core/memory/Any.hpp>
#include <core/memory/UniquePtr.hpp>

#include <script/vm/VMString.hpp>
#include <core/Types.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/Defines.hpp>

#include <core/HashCode.hpp>

#include <sstream>

namespace hyperion {

class APIInstance;
struct HypData;

namespace vm {

typedef uint32 BCAddress;
typedef uint8 BCRegister;

struct Value;

class InstructionHandler;
struct Script_ExecutionThread;
class HeapValue;
struct VMState;

struct Number
{
    using Flags = uint32;

    enum FlagBits : Flags
    {
        FLAG_NONE = 0x00,
        FLAG_SIGNED = 0x01,
        FLAG_UNSIGNED = 0x02,
        FLAG_FLOATING_POINT = 0x04,

        FLAG_8_BIT = 0x08,
        FLAG_16_BIT = 0x10,
        FLAG_32_BIT = 0x20,
        FLAG_64_BIT = 0x40
    };

    union
    {
        int64 i;
        uint64 u;
        double f;
    };

    Flags flags;
};

} // namespace vm

namespace sdk {

struct Params
{
    APIInstance& apiInstance;
    vm::InstructionHandler* handler;
    vm::Value** args;
    int32 nargs;
};

} // namespace sdk
} // namespace hyperion

// native typedefs
typedef void (*Script_NativeFunction)(hyperion::sdk::Params);
typedef void (*Script_Initializer)(hyperion::vm::VMState*, hyperion::vm::Script_ExecutionThread* thread, hyperion::vm::Value*);
typedef void* Script_UserData;

namespace hyperion {
namespace vm {

TypeId GetTypeIdForHeapValue(const HeapValue*);
void* GetRawPointerForHeapValue(HeapValue*);
const void* GetRawPointerForHeapValue(const HeapValue*);

enum CompareFlags : uint8
{
    NONE = 0x00,
    EQUAL = 0x01,
    GREATER = 0x02
};

struct alignas(8) Script_VMData
{
    union
    {
        Value* valueRef;

        HeapValue* ptr;

        struct
        {
            BCAddress m_addr;
            uint8 m_nargs;
            uint8 m_flags;
        } func;

        Script_NativeFunction nativeFunc;
        Script_UserData userData;

        struct
        {
            BCAddress returnAddress;
            int32 varargsPush;
        } call;

        BCAddress addr;

        struct
        {
            BCAddress catchAddress;
        } tryCatchInfo;

        struct
        {
            const char* errorMessage; // make sure it is a string literal, as it is not managed
        } invalidStateObject;
    };

    enum
    {
        VALUE_REF,
        HEAP_POINTER,
        FUNCTION,
        NATIVE_FUNCTION,
        USER_DATA,
        ADDRESS,
        FUNCTION_CALL,
        TRY_CATCH_INFO,
        INVALID_STATE_OBJECT // used for error handling in native functions
    } type;
};

class alignas(8) Value
{
    char m_internal[40];

    HypData* GetHypData();
    const HypData* GetHypData() const;

public:
    Value();

    explicit Value(HypData&& data);
    explicit Value(const Script_VMData& vmData);

    Value(const Value& other) = delete;
    Value(Value&& other) noexcept;

    Value& operator=(const Value& other) = delete;
    Value& operator=(Value&& other) noexcept;

    Script_VMData* GetVMData() const;

    bool IsRef() const;
    Value* GetRef() const;

    void AssignValue(Value&& other, bool assignRef);

    bool GetUnsigned(uint64* out) const;
    bool GetInteger(int64* out) const;
    bool GetSignedOrUnsigned(Number* out) const;

    bool GetFloatingPoint(double* out) const;
    bool GetFloatingPointCoerce(double* out) const;

    bool GetNumber(double* out) const;
    bool GetNumber(Number* out) const;

    bool GetBoolean(bool* out) const;

    AnyRef ToRef() const;

    Script_UserData GetUserData() const;

    static int CompareAsPointers(Value* lhs, Value* rhs);
    static int CompareAsFunctions(Value* lhs, Value* rhs);
    static int CompareAsNativeFunctions(Value* lhs, Value* rhs);

#if 0
    Any ToAny() const;
    AnyPtr ToAnyPtr() const;
#endif

    void Mark();

    const char* GetTypeString() const;
    VMString ToString() const;
    void ToRepresentation(
        std::stringstream& ss,
        bool addTypeName = true,
        int depth = 3) const;
};

} // namespace vm
} // namespace hyperion
