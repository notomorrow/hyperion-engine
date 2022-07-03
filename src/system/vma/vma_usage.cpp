#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "../debug.h"

#ifdef HYP_LOG_MEMORY_OPERATIONS
#define VMA_DEBUG_LOG(...) \
    {   DebugLog(LogType::RenInfo, __VA_ARGS__); \
        puts("");  }
#endif


#include "vma_usage.h"
