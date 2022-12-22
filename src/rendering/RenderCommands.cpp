#include <rendering/RenderCommands.hpp>

namespace hyperion::v2 {

HeapArray<RenderCommands::HolderRef, RenderCommands::max_render_command_types> RenderCommands::holders = { };
std::atomic<SizeType> RenderCommands::render_command_type_index = { 0 };
RenderScheduler RenderCommands::scheduler = { };

std::mutex RenderCommands::mtx = std::mutex();
std::condition_variable RenderCommands::flushed_cv = std::condition_variable();

void RenderScheduler::Commit(RenderCommand *ptr)
{
    m_commands.PushBack(ptr);
    m_num_enqueued.fetch_add(1, std::memory_order_relaxed);
}

RenderScheduler::FlushResult RenderScheduler::Flush()
{
    Threads::AssertOnThread(m_owner_thread);

    FlushResult result { Result::OK, 0 };

    SizeType count = m_num_enqueued.load(std::memory_order_relaxed);

    while (count) {
        RenderCommand *front = m_commands.Front();
        --count;

        ++result.num_executed;

#ifdef HYP_DEBUG_RENDER_COMMANDS
        DebugLog(LogType::RenDebug, "Executing render command %s\n", typeid(*front).name());
#endif

        result.result = (*front)();
        front->~RenderCommand();

        m_commands.PopFront();

        // check if an error occurred
        if (!result.result) {
            DebugLog(LogType::Error, "Error! %s\n", result.result.message);

            while (m_commands.Any()) {
                m_commands.PopFront()->~RenderCommand();
            }

            m_num_enqueued.store(count, std::memory_order_relaxed);

            break;
        }
    }

    m_num_enqueued.store(0, std::memory_order_relaxed);

    return result;
}

void RenderCommands::Rewind()
{
    // all items in the cache must have had destructor called on them already.

    HolderRef *p = &holders[0];

    while (*p) {
        const SizeType counter_value = p->counter_ptr->load();

        if (counter_value) {
            Memory::Set(p->memory_ptr, 0x00, p->object_size * counter_value);

            p->counter_ptr->store(0);
        }
        ++p;
    }
}

} // namespace hyperion::v2