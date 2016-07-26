#include "core_engine.h"
#include <exception>

namespace apex {
CoreEngine *CoreEngine::instance = nullptr;

CoreEngine *CoreEngine::GetInstance()
{
    if (instance == nullptr) {
        throw std::exception("no engine instance created");
    }
    return instance;
}

void CoreEngine::SetInstance(CoreEngine *ptr)
{
    instance = ptr;
}
}