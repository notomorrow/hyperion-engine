/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Debug/StackDump.hpp>

#include <Core/Memory/Memory.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/Logging/Logger.hpp>

#if HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>
#elif HYP_UNIX && !HYP_ANDROID && !HYP_WEB
#include <execinfo.h>
#endif

namespace Hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(StackTrace, Core);

namespace debug {

struct StackDump::Impl
{
    Array<void*> rawFrames;
    mutable Array<String> stringCache;
    mutable bool isCacheValid = false;

    Impl() = default;

    Impl(const Impl& other)
        : rawFrames(other.rawFrames),
          stringCache(other.stringCache),
          isCacheValid(other.isCacheValid)
    {
    }

    Impl& operator=(const Impl& other)
    {
        if (this != &other)
        {
            rawFrames = other.rawFrames;
            stringCache = other.stringCache;
            isCacheValid = other.isCacheValid;
        }

        return *this;
    }

    void EnsureStringCache() const
    {
        if (isCacheValid)
        {
            return;
        }

        stringCache.Resize(0);
        stringCache.Reserve(rawFrames.Size());

#if HYP_WINDOWS
        HANDLE process = GetCurrentProcess();
        SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES);
        SymInitialize(process, nullptr, true);

        for (void* frame : rawFrames)
        {
            DWORD64 address = reinterpret_cast<DWORD64>(frame);

            DWORD64 displacementSym = 0;
            char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
            PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;

            if (SymFromAddr(process, address, &displacementSym, symbol))
            {
                char line[2048];

                IMAGEHLP_LINE64 sourceLine = {};
                sourceLine.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
                DWORD displacementLine = 0;

                if (SymGetLineFromAddr64(process, address, &displacementLine, &sourceLine))
                {
                    snprintf(line, 2048, "%s+0x%llX - 0x%0llX (%s:%lu)", symbol->Name, displacementSym, symbol->Address, sourceLine.FileName, sourceLine.LineNumber);
                }
                else
                {
                    snprintf(line, 2048, "%s+0x%llX - 0x%0llX", symbol->Name, displacementSym, symbol->Address);
                }

                stringCache.PushBack(line);

                continue;
            }

            // without symbols, the module and offset still say where it is (no module: e.g code of an unloaded DLL)
            HMODULE module = nullptr;
            char modulePath[MAX_PATH] = {};

            char line[2048];

            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &module)
                && GetModuleFileNameA(module, modulePath, MAX_PATH) != 0)
            {
                const char* moduleName = strrchr(modulePath, '\\');

                snprintf(line, 2048, "(unknown) %s+0x%llX", moduleName ? moduleName + 1 : modulePath, address - reinterpret_cast<DWORD64>(module));
            }
            else
            {
                snprintf(line, 2048, "(unknown) 0x%llX, in no loaded module", address);
            }

            stringCache.PushBack(line);
        }

        SymCleanup(process);
#elif HYP_UNIX && !HYP_ANDROID && !HYP_WEB
        if (rawFrames.Any())
        {
            char** strings = backtrace_symbols(rawFrames.Data(), int(rawFrames.Size()));

            if (strings != nullptr)
            {
                for (size_t i = 0; i < rawFrames.Size(); ++i)
                {
                    stringCache.PushBack(strings[i]);
                }

                free(strings);
            }
        }
#else
        stringCache.PushBack("Stack trace not supported on this platform.");
#endif

        isCacheValid = true;
    }
};

static void CaptureRawStackFrames(Array<void*>& rawFrames, uint32 depth, uint32 offset, const void* fromContext = nullptr)
{
    rawFrames.Clear();
    rawFrames.Reserve(depth);

#if HYP_WINDOWS
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, true);

    CONTEXT context = {};

    if (fromContext != nullptr)
    {
        context = *static_cast<const CONTEXT*>(fromContext);
    }
    else
    {
        context.ContextFlags = CONTEXT_FULL;
        RtlCaptureContext(&context);
    }

    STACKFRAME64 stackFrame = {};
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;

#ifdef HYP_ARM
    DWORD machineType = IMAGE_FILE_MACHINE_ARM64;
    stackFrame.AddrPC.Offset = context.Pc;
    stackFrame.AddrFrame.Offset = context.Fp;
    stackFrame.AddrStack.Offset = context.Sp;
#else
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrFrame.Offset = context.Rbp;
    stackFrame.AddrStack.Offset = context.Rsp;
#endif

    uint32 index = 0;

    while (index < depth && StackWalk64(machineType, process, GetCurrentThread(), &stackFrame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
    {
        DWORD64 address = stackFrame.AddrPC.Offset;
        if (address == 0)
        {
            break;
        }

        rawFrames.PushBack(reinterpret_cast<void*>(address));
        index++;
    }

    SymCleanup(process);
#elif HYP_UNIX && !HYP_ANDROID && !HYP_WEB
    offset += 2;

    void** stack = (void**)malloc((depth + offset) * sizeof(void*));
    const int frames = backtrace(stack, depth + offset);

    if (frames - int(offset) > 0)
    {
        for (int i = offset; i < frames; ++i)
        {
            rawFrames.PushBack(stack[i]);
        }
    }

    free(stack);
#else
    // Platform not supported - will be handled in string conversion
#endif
}

RawStackTrace::RawStackTrace(uint32 depth, uint32 offset)
    : numFrames(0)
{
    Array<void*> tempFrames;
    CaptureRawStackFrames(tempFrames, MathUtil::Min(depth, MaxFrames), offset);

    numFrames = MathUtil::Min(uint32(tempFrames.Size()), MaxFrames);
    for (uint32 i = 0; i < numFrames; ++i)
    {
        frames[i] = tempFrames[i];
    }
}

StackDump* RawStackTrace::ToStackDump() const
{
    return new StackDump(*this);
}

StackDump::StackDump(uint32 depth, uint32 offset)
{
    m_impl.Emplace();
    CaptureRawStackFrames(m_impl->rawFrames, depth, offset);
}

StackDump::StackDump(const void* context, uint32 depth)
{
    m_impl.Emplace();
    CaptureRawStackFrames(m_impl->rawFrames, depth, 0, context);
}

StackDump::StackDump(const RawStackTrace& rawTrace)
{
    m_impl.Emplace();
    m_impl->rawFrames.Reserve(rawTrace.numFrames);
    for (uint32 i = 0; i < rawTrace.numFrames; ++i)
    {
        m_impl->rawFrames.PushBack(rawTrace.frames[i]);
    }
}

StackDump::StackDump(const StackDump& other)
{
    if (other.m_impl)
    {
        m_impl.Emplace(*other.m_impl);
    }
}

StackDump& StackDump::operator=(const StackDump& other)
{
    if (this != &other)
    {
        if (other.m_impl)
        {
            if (m_impl)
            {
                *m_impl = *other.m_impl;
            }
            else
            {
                m_impl.Emplace(*other.m_impl);
            }
        }
        else
        {
            m_impl.Reset();
        }
    }

    return *this;
}

StackDump::~StackDump() = default;

const Array<String>& StackDump::GetTrace() const
{
    if (m_impl)
    {
        m_impl->EnsureStringCache();
        return m_impl->stringCache;
    }

    static const Array<String> s_emptyArray;
    return s_emptyArray;
}

String StackDump::ToString() const
{
    return String::Join(GetTrace(), "\n");
}

// Implementation of global LogStackTrace() function from Defines.hpp
void LogStackTrace(int depth)
{
    HYP_LOG(StackTrace, Debug, "Stack trace:\n\n{}", StackDump(depth, 1).ToString());
}

} // namespace debug
} // namespace Hyperion
