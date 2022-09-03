#include "BLAS.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

Blas::Blas(Handle<Mesh> &&mesh, const Transform &transform)
    : EngineComponentWrapper(),
      m_mesh(std::move(mesh)),
      m_transform(transform)
{
}

Blas::~Blas()
{
    Teardown();
}

void Blas::SetMesh(Handle<Mesh> &&mesh)
{
    m_mesh = std::move(mesh);

    if (!m_wrapped.GetGeometries().empty()) {
        auto size = static_cast<Int64>(m_wrapped.GetGeometries().size());

        while (size) {
            m_wrapped.RemoveGeometry(size--);
        }
    }

    if (m_mesh && IsInitCalled()) {
        GetEngine()->InitObject(m_mesh);
        
        m_wrapped.AddGeometry(std::make_unique<AccelerationGeometry>(
            m_mesh->BuildPackedVertices(),
            m_mesh->BuildPackedIndices()
        ));
    }
}

void Blas::SetTransform(const Transform &transform)
{
    m_transform = transform;

    if (IsInitCalled()) {
        m_wrapped.SetTransform(m_transform.GetMatrix());
    }
}

void Blas::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    if (m_mesh) {
        engine->InitObject(m_mesh);

        m_wrapped.SetTransform(m_transform.GetMatrix());
        m_wrapped.AddGeometry(std::make_unique<AccelerationGeometry>(
            m_mesh->BuildPackedVertices(),
            m_mesh->BuildPackedIndices()
        ));
    }

    EngineComponentWrapper::Create(
        engine,
        engine->GetInstance()
    );

    OnTeardown([this]() {
        EngineComponentWrapper::Destroy(GetEngine());

        m_mesh.Reset();
    });
}

void Blas::Update(Engine *engine)
{
    if (!IsInitCalled()) {
        return;
    }

    if (!NeedsUpdate()) {
        return;
    }

    HYPERION_ASSERT_RESULT(m_wrapped.UpdateStructure(engine->GetInstance()));
}

} // namespace hyperion::v2