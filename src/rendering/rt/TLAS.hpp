/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TLAS_HPP
#define HYPERION_TLAS_HPP

#include <core/Base.hpp>

#include <rendering/rt/BLAS.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <mutex>
#include <atomic>

namespace hyperion {

using renderer::TopLevelAccelerationStructure;
using renderer::RTUpdateStateFlags;

class HYP_API TLAS : public BasicObject<STUB_CLASS(TLAS)>
{
public:
    TLAS();
    TLAS(const TLAS &other)             = delete;
    TLAS &operator=(const TLAS &other)  = delete;
    ~TLAS();
    
    const TLASRef &GetInternalTLAS() const
        { return m_tlas; }

    void AddBLAS(Handle<BLAS> blas);
    void RemoveBLAS(ID<BLAS> blas_id);

    void Init();

    /*! \brief Update the TLAS on the RENDER thread, performing any
     * updates to the structure. This is also called for each BLAS underneath this.
     */
    void UpdateRender(
        Frame *frame,
        RTUpdateStateFlags &out_flags
    );

private:
    void PerformBLASUpdates();

    TLASRef             m_tlas;

    Array<Handle<BLAS>> m_blas;
    Array<Handle<BLAS>> m_blas_pending_addition;
    Array<ID<BLAS>>     m_blas_pending_removal;

    std::atomic_bool    m_has_blas_updates { false };
    std::mutex          m_blas_updates_mutex;
};

} // namespace hyperion

#endif