#include <rendering/font/FontEngine.hpp>
#include <rendering/font/Face.hpp>

#ifdef HYP_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H

#endif

namespace hyperion::v2 {

Face::Face(FontEngine::Backend backend, const FilePath &path)
    : m_face(nullptr)
{
#ifdef HYP_FREETYPE
    if (FT_New_Face(backend, path.Data(), 0, &m_face)) {
        m_face = nullptr;
        return;
    }
#endif
}

Face::Face(Face &&other) noexcept
    : m_face(other.m_face)
{
    other.m_face = nullptr;
}

Face &Face::operator=(Face &&other) noexcept
{
    if (this != &other) {
        m_face = other.m_face;
        other.m_face = nullptr;
    }

    return *this;
}

Face::~Face()
{
}

void Face::SetGlyphSize(int pt_w, int pt_h, int screen_width, int screen_height)
{
#ifdef HYP_FREETYPE
    int error = FT_Set_Char_Size(m_face, pt_w * 64, pt_h * 64, screen_width, screen_height);
    if (error) {
        DebugLog(LogType::Error, "Error could not set glyph size\n");
    }
#endif
}

void Face::RequestPixelSizes(int width, int height)
{
#ifdef HYP_FREETYPE
    if (FT_Set_Pixel_Sizes(m_face, width, height)) {
        DebugLog(LogType::Error, "Error! could not set the height of fontface to %u, %u\n", width, height);
    }
#endif
}

Face::GlyphIndex Face::GetGlyphIndex(WChar to_find)
{
#ifdef HYP_FREETYPE
    return FT_Get_Char_Index(m_face, to_find);
#else
    return -1;
#endif
}

FontEngine::Font Face::GetFace()
{
    return m_face;
}

} // namespace hyperion::v2::font