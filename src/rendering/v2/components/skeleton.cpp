#include "skeleton.h"
#include "bone.h"
#include "../engine.h"

namespace hyperion::v2 {

Skeleton::Skeleton()
    : EngineComponentBase(),
      m_root_bone(nullptr),
      m_shader_data_state(ShaderDataState::DIRTY)
{
}

Skeleton::Skeleton(std::unique_ptr<Bone> &&root_bone)
    : EngineComponentBase(),
      m_root_bone(std::move(root_bone))
{
}

Skeleton::~Skeleton()
{
    Teardown();
}

void Skeleton::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SKELETONS, [this](Engine *engine) {
        UpdateShaderData(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SKELETONS, [this](Engine *engine) {

        }), engine);
    }));
}

void Skeleton::UpdateShaderData(Engine *engine) const
{
    /* TODO: store somewhere rather than allocing on stack
     * as this struct can get large
     */
    SkeletonShaderData shader_data{};

    const size_t num_bones = MathUtil::Min(SkeletonShaderData::max_bones, NumBones());

    if (num_bones != 0) {
        shader_data.bones[0] = m_root_bone->GetBoneMatrix();

        for (size_t i = 1; i < num_bones; i++) {
            if (auto *descendent = m_root_bone->GetDescendents()[i - 1]) {
                if (descendent->GetType() != Node::Type::BONE) {
                    continue;
                }

                shader_data.bones[i] = static_cast<Bone *>(descendent)->GetBoneMatrix();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            }
        }

        engine->shader_globals->skeletons.Set(m_id.value - 1, std::move(shader_data));
    }
    
    m_shader_data_state = ShaderDataState::CLEAN;
}

Bone *Skeleton::FindBone(const char *name) const
{
    if (m_root_bone == nullptr) {
        return nullptr;
    }

    if (!std::strcmp(m_root_bone->GetTag(), name)) {
        return m_root_bone.get();
    }

    for (Node *node : m_root_bone->GetDescendents()) {
        if (node == nullptr || node->GetType() != Node::Type::BONE) {
            continue;
        }

        Bone *bone = static_cast<Bone *>(node);

        if (!std::strcmp(bone->GetTag(), name)) {
            return bone;
        }
    }

    return nullptr;
}

void Skeleton::SetRootBone(std::unique_ptr<Bone> &&root_bone)
{
    m_root_bone = std::move(root_bone);
}

size_t Skeleton::NumBones() const
{
    if (m_root_bone == nullptr) {
        return 0;
    }

    return 1 + m_root_bone->GetDescendents().size();
}

} // namespace hyperion::v2