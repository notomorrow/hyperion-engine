#ifndef UI_OBJECT_H
#define UI_OBJECT_H

#include "../../scene/node.h"
#include "../../input_manager.h"

namespace hyperion {
class Texture;
namespace ui {
class UIObject : public Node {
public:
    UIObject(const std::string &name = "");
    virtual ~UIObject() = default;

    void UpdateTransform() override;

    bool IsMouseOver(double x, double y) const;

    InputEvent &GetClickEvent() { return m_click_event; }
    const InputEvent &GetClickEvent() const { return m_click_event; }
    void SetClickEvent(const InputEvent &input_event) { m_click_event = input_event; }

    InputEvent &GetHoverEvent() { return m_hover_event; }
    const InputEvent &GetHoverEvent() const { return m_hover_event; }
    void SetHoverEvent(const InputEvent &input_event) { m_hover_event = input_event; }

    inline std::shared_ptr<Texture> GetImage() const
        { return GetMaterial().GetTexture(MATERIAL_TEXTURE_COLOR_MAP); }
    inline void SetImage(std::shared_ptr<Texture> texture)
        { GetMaterial().SetTexture(MATERIAL_TEXTURE_COLOR_MAP, texture); }

    inline void SetLocalTranslation2D(const Vector2 &translation)   
        { SetLocalTranslation(Vector3(translation.x, translation.y, m_local_translation.z)); }
    inline Vector2 GetLocalTranslation2D() const
        { return Vector2(m_local_translation.x, m_local_translation.y); }

    inline void SetLocalScale2D(const Vector2 &scale)   
        { SetLocalScale(Vector3(scale.x, scale.y, m_local_scale.z)); }
    inline Vector2 GetLocalScale2D() const
        { return Vector2(m_local_scale.x, m_local_scale.y); }

protected:
    InputEvent m_click_event;
    InputEvent m_hover_event;
};
} // namespace ui
} // namespace hyperion

#endif
