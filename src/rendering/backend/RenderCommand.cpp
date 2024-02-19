#include <rendering/backend/RenderCommand.hpp>

#include <Threads.hpp>

namespace hyperion::renderer {

using ::hyperion::v2::Threads;

FixedArray<RenderCommandHolder, max_render_command_types> RenderCommands::holders = { };
AtomicVar<SizeType> RenderCommands::render_command_type_index = { 0 };
RenderScheduler RenderCommands::scheduler = { };

std::mutex RenderCommands::mtx = std::mutex();
std::condition_variable RenderCommands::flushed_cv = std::condition_variable();

uint RenderCommands::buffer_index = 0;

void RenderScheduler::Commit(RenderCommand *command)
{
    m_commands.PushBack(command);
    m_num_enqueued.Increment(1, MemoryOrder::RELAXED);
}

void RenderScheduler::AcceptAll(Array<RenderCommand *> &out_container)
{
    out_container = std::move(m_commands);
    m_num_enqueued.Set(0, MemoryOrder::RELAXED);
}

Result RenderCommands::Flush()
{
    if (Count() == 0) {
        HYPERION_RETURN_OK;
    }

    Threads::AssertOnThread(v2::THREAD_RENDER);

#if 1
    Array<RenderCommand *> commands;

    // Lock in order to accept commands, so when
    // one of our render commands pushes to the queue,
    // it will not cause a deadlock.
    // Also it will be more performant this way as less time will be spent
    // in the locked section.
    std::unique_lock lock(mtx);

    scheduler.AcceptAll(commands);

    const SizeType num_commands = commands.Size();
    scheduler.m_num_enqueued.Decrement(num_commands, MemoryOrder::RELAXED);

    // Swap buffers before executing commands, so that the commands may enqueue new commands
    SwapBuffers();

    lock.unlock();

    flushed_cv.notify_all();


    for (SizeType index = 0; index < num_commands; index++) {
        RenderCommand *front = commands[index];

#ifdef HYP_DEBUG_LOG_RENDER_COMMANDS
        DebugLog(LogType::RenDebug, "Executing render command %s\n", front->_debug_name.Data());
#endif

        const Result command_result = front->Call();
        AssertThrowMsg(command_result, "Render command error! [%d]: %s\n", command_result.error_code, command_result.message);
        front->~RenderCommand();
    }

    if (num_commands) {
        Rewind();
    }
#else

    std::unique_lock lock(mtx);
    
    const RenderScheduler::FlushResult flush_result = scheduler.Flush();

    if (flush_result.num_executed) {
        Rewind();

        scheduler.m_num_enqueued.Decrement(flush_result.num_executed, MemoryOrder::RELAXED);
    }
    
    lock.unlock();

    flushed_cv.notify_all();
#endif

    return Result::OK;
}

Result RenderCommands::FlushOrWait()
{
    if (Count() == 0) {
        HYPERION_RETURN_OK;
    }

    if (Threads::IsOnThread(v2::THREAD_RENDER)) {
        return Flush();
    }

    Wait();

    HYPERION_RETURN_OK;
}

void RenderCommands::Wait()
{
    Threads::AssertOnThread(~v2::THREAD_RENDER);

    std::unique_lock lock(mtx);
    flushed_cv.wait(lock, [] { return RenderCommands::Count() == 0; });
}

void RenderCommands::SwapBuffers()
{
    buffer_index = (buffer_index + 1) & 1;
}

void RenderCommands::Rewind()
{
    // Use previous buffer index since we call SwapBuffers before executing commands.
    const uint prev_buffer_index = (buffer_index + 1) & 1;

    // all items in the cache must have had destructor called on them already.

    for (auto it = holders.Begin(); it != holders.End(); ++it) {
        if (!it->render_command_list_ptr) {
            break;
        }

        it->rewind_func(it->render_command_list_ptr, prev_buffer_index);
    }
}

} // namespace hyperion::renderer