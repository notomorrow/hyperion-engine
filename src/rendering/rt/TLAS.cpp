#include "TLAS.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Result;

struct RENDER_COMMAND(CreateTLAS) : renderer::RenderCommand
{
    renderer::TopLevelAccelerationStructure *tlas;
    Array<renderer::BottomLevelAccelerationStructure *> blases;

    RENDER_COMMAND(CreateTLAS)(renderer::TopLevelAccelerationStructure *tlas, Array<renderer::BottomLevelAccelerationStructure *> &&blases)
        : tlas(tlas),
          blases(std::move(blases))
    {
    }

    virtual Result operator()()
    {
        return tlas->Create(
            g_engine->GetGPUDevice(),
            g_engine->GetGPUInstance(),
            std::vector<renderer::BottomLevelAccelerationStructure *>(blases.Begin(), blases.End())
        );
    }
};

struct RENDER_COMMAND(DestroyTLAS) : renderer::RenderCommand
{
    renderer::TopLevelAccelerationStructure *tlas;

    RENDER_COMMAND(DestroyTLAS)(renderer::TopLevelAccelerationStructure *tlas)
        : tlas(tlas)
    {
    }

    virtual Result operator()()
    {
        return tlas->Destroy(g_engine->GetGPUDevice());
    }
};

TLAS::TLAS()
    : BasicObject()
{
}

TLAS::~TLAS()
{
    Teardown();
}

void TLAS::AddBLAS(Handle<BLAS> blas)
{
    if (!blas) {
        return;
    }

    if (IsInitCalled()) {
        if (!InitObject(blas)) {
            // the blas could not be initialzied. not valid?
            return;
        }
    }

    std::lock_guard guard(m_blas_updates_mutex);

    m_blas_pending_addition.PushBack(std::move(blas));
    m_has_blas_updates.store(true);
}

void TLAS::RemoveBLAS(ID<BLAS> blas_id)
{
    if (IsInitCalled()) {
        std::lock_guard guard(m_blas_updates_mutex);

        m_blas_pending_removal.PushBack(blas_id);
        m_has_blas_updates.store(true);
    }
}

void TLAS::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();
    
    // add all pending to be added to the list
    if (m_has_blas_updates.load()) {
        std::lock_guard guard(m_blas_updates_mutex);

        m_blas.Concat(std::move(m_blas_pending_addition));

        for (ID<BLAS> blas_id : m_blas_pending_removal) {
            auto it = m_blas.FindIf([blas_id](const Handle<BLAS> &blas) -> bool
            {
                return blas.GetID() == blas_id;
            });

            if (it != m_blas.End()) {
                m_blas.Erase(it);
            }
        }

        m_has_blas_updates.store(false);
    }

    for (SizeType i = 0; i < m_blas.Size(); i++) {
        AssertThrow(m_blas[i] != nullptr);
        AssertThrow(InitObject(m_blas[i]));
    }

    Array<BottomLevelAccelerationStructure *> internal_blases;
    internal_blases.Resize(m_blas.Size());

    for (SizeType i = 0; i < m_blas.Size(); i++) {
        internal_blases[i] = &m_blas[i]->GetInternalBLAS();
    }

    PUSH_RENDER_COMMAND(CreateTLAS, &m_tlas, std::move(internal_blases));

    HYP_SYNC_RENDER();

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        m_blas.Clear();

        if (m_has_blas_updates.load()) {
            m_blas_updates_mutex.lock();
            m_blas_pending_addition.Clear();
            m_blas_updates_mutex.unlock();

            m_has_blas_updates.store(false);
        }

        PUSH_RENDER_COMMAND(DestroyTLAS, &m_tlas);

        HYP_SYNC_RENDER();
    });
}

void TLAS::PerformBLASUpdates()
{
    std::lock_guard guard(m_blas_updates_mutex);

    while (m_blas_pending_addition.Any()) {
        auto blas = m_blas_pending_addition.PopFront();
        AssertThrow(blas);

        m_tlas.AddBLAS(&blas->GetInternalBLAS());

        m_blas.PushBack(std::move(blas));
    }

    m_has_blas_updates.store(false);
}

void TLAS::UpdateRender(
    Frame *frame,
    RTUpdateStateFlags &out_update_state_flags
)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    out_update_state_flags = renderer::RT_UPDATE_STATE_FLAGS_NONE;

    if (m_has_blas_updates.load()) {
        PerformBLASUpdates();
    }
    
    for (auto &blas : m_blas) {
        AssertThrow(blas != nullptr);
        
        AssertThrow(!blas->GetInternalBLAS().GetGeometries().empty());
        AssertThrow(blas->GetInternalBLAS().GetGeometries()[0]->GetPackedIndexStorageBuffer() != nullptr);
        AssertThrow(blas->GetInternalBLAS().GetGeometries()[0]->GetPackedVertexStorageBuffer() != nullptr);

        //bool was_blas_rebuilt = false;
        //blas->UpdateRenderframe, was_blas_rebuilt);
    }
    
    HYPERION_ASSERT_RESULT(m_tlas.UpdateStructure(g_engine->GetGPUInstance(), out_update_state_flags));
}

} // namespace hyperion::v2