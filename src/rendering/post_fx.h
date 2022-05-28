#ifndef HYPERION_V2_POST_FX_H
#define HYPERION_V2_POST_FX_H

#include "render_pass.h"
#include "framebuffer.h"
#include "shader.h"
#include "graphics.h"
#include "mesh.h"
#include "full_screen_pass.h"

#include <rendering/backend/renderer_frame.h>
#include <rendering/backend/renderer_structs.h>
#include <rendering/backend/renderer_command_buffer.h>

#include <core/lib/type_map.h>

#include <memory>
#include <utility>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::CommandBuffer;
using renderer::PerFrameData;
using renderer::VertexAttributeSet;

class Engine;

class PostProcessingEffect : public EngineComponentBase<STUB_CLASS(PostProcessingEffect)> {
public:
    PostProcessingEffect();
    PostProcessingEffect(const PostProcessingEffect &other) = delete;
    PostProcessingEffect &operator=(const PostProcessingEffect &other) = delete;
    virtual ~PostProcessingEffect();

    FullScreenPass &GetFullScreenPass()             { return m_full_screen_pass; }
    const FullScreenPass &GetFullScreenPass() const { return m_full_screen_pass; }

    Ref<Shader> &GetShader()                        { return m_shader; }
    const Ref<Shader> &GetShader() const            { return m_shader; }

    void Init(Engine *engine);

protected:
    virtual Ref<Shader> CreateShader(Engine *engine) = 0;

    FullScreenPass m_full_screen_pass;

private:
    Ref<Shader>    m_shader;
};

class PostProcessing {
public:
    PostProcessing();
    PostProcessing(const PostProcessing &) = delete;
    PostProcessing &operator=(const PostProcessing &) = delete;
    ~PostProcessing();

    /*! \brief Add an effect to the stack.
    * Note, cannot add new filters after pipeline construction, currently
     * @param effect A unique_ptr to the class, derived from PostProcessingEffect.
     */
    template <class EffectClass>
    void AddFilter(std::unique_ptr<EffectClass> &&effect)
    {
        static_assert(std::is_base_of_v<PostProcessingEffect, EffectClass>, "Type must be a derived class of PostProcessingEffect.");

        m_effects.Set<EffectClass>(std::move(effect));
    }

    /*! \brief Add an effect to the stack, constructed by the given arguments.
     * Note, cannot add new filters after pipeline construction, currently
     */
    template <class EffectClass, class ...Args>
    void AddFilter(Args &&... args)
    {
        static_assert(std::is_base_of_v<PostProcessingEffect, EffectClass>, "Type must be a derived class of PostProcessingEffect.");

        AddFilter(std::make_unique<EffectClass>(std::forward<Args>(args)...));
    }

    template <class EffectClass>
    EffectClass *GetFilter() const
    {
        static_assert(std::is_base_of_v<PostProcessingEffect, EffectClass>, "Type must be a derived class of PostProcessingEffect.");

        auto it = m_effects.Find<EffectClass>();

        if (it == m_effects.End()) {
            return nullptr;
        }

        return static_cast<EffectClass *>(it->second.get());
    }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame) const;

private:
    TypeMap<std::unique_ptr<PostProcessingEffect>> m_effects;
};

} // namespace hyperion::v2

#endif // HYPERION_V2_POST_FX_H

