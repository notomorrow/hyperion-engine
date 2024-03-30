#ifndef HYP_FONT_FONTGLYPH_HPP
#define HYP_FONT_FONTGLYPH_HPP

#include <rendering/font/FontEngine.hpp>
#include <rendering/font/Face.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/Texture.hpp>

namespace hyperion::v2 {

class Glyph
{
public:

    HYP_PACK_BEGIN
    struct PackedMetrics
    {
        uint16  width;     /* 2 */
        uint16  height;    /* 4 */
        int16   bearing_x;  /* 6 */
        int16   bearing_y;  /* 8 */
        uint8   advance;    /* 9 */

        uint8   _reserved0;  /* 10 */
    } HYP_PACK_END;


    struct Metrics
    {
        PackedMetrics   metrics;
        Vec2i           image_position;

        [[nodiscard]]
        PackedMetrics GetPackedMetrics() const
            { return metrics; }
    };

    Glyph(RC<Face> face, Face::GlyphIndex index, bool render);
    void Render();

    [[nodiscard]] Metrics GetMetrics() const
        { return m_metrics; }

    Extent2D GetMax();
    Extent2D GetMin();
    const Handle<Texture> &GetTexture() const;

private:
    RC<Face>                    m_face;

    FontEngine::Glyph           m_glyph;
    Handle<Texture>             m_texture;
    Metrics                     m_metrics { 0 };
};

}; // namespace hyperion::v2

#endif //HYP_FONT_FONTGLYPH_HPP
