#ifndef GI_MAPPING_H
#define GI_MAPPING_H

#include <memory>

namespace hyperion {
class GIMapping {
public:
    GIMapping();
    ~GIMapping();

    inline unsigned int GetTextureId() const { return m_texture_id; }

    void Begin();
    void End();

private:
    unsigned int m_texture_id;
};
} // namespace apex

#endif