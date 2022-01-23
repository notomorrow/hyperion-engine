#ifndef UI_TEXT_H
#define UI_TEXT_H

#include "ui_object.h"
#include "../../util.h"
#include "../../math/vector2.h"
#include "../texture_2D.h"

#include <array>
#include <memory>

namespace hyperion {
class Mesh;
namespace ui {
class FontMap {
public:
    std::shared_ptr<Texture2D> m_texture;
    std::array<Vector2, 255> m_char_texture_coords;
    Vector2 m_char_size;

    FontMap(const std::shared_ptr<Texture2D> &texture,
        int num_chars_per_row,
        int num_chars_per_col,
        int char_offset = 0);
    ~FontMap() = default;

    inline const std::shared_ptr<Texture2D> &GetTexture() const { return m_texture; }

    inline Vector2 GetCharOffset(char ch) const
    {
        return m_char_texture_coords[ch] / Vector2(NumCharsPerRow(), NumCharsPerCol());
    }

    inline Vector2 GetScaling() const
    {
        ex_assert(m_texture != nullptr);

        return m_char_size / Vector2(m_texture->GetWidth(), m_texture->GetHeight());
    }

    inline int NumCharsPerRow() const
    {
        ex_assert(m_texture != nullptr);

        return m_texture->GetWidth() / int(m_char_size.x);
    }

    inline int NumCharsPerCol() const
    {
        ex_assert(m_texture != nullptr);

        return m_texture->GetHeight() / int(m_char_size.y);
    }
};

class UIText : public UIObject {
public:
    UIText(const std::string &name = "", const std::string &text = "");
    virtual ~UIText();

    inline const std::string &GetText() const { return m_text; }
    inline void SetText(const std::string &text) { m_text = text; }

    struct CharMesh {
        std::shared_ptr<Mesh> quad;
        Transform transform;
    };

protected:
    std::string m_text;
    FontMap *m_font_map;

    std::vector<CharMesh> BuildCharMeshes() const;
    CharMesh OptimizeCharMeshes(std::vector<CharMesh> char_meshes) const;
};
} // namespace ui
} // namespace hyperion


#endif
