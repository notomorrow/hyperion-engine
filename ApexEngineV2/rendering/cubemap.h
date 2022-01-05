#ifndef CUBEMAP_H
#define CUBEMAP_H

#include <array>
#include <vector>
#include <memory>

#include "./texture_2D.h"

#define CUBEMAP_NUM_MIPMAPS 5

namespace apex {

class Cubemap : public Texture {
public:
    struct MipMap {
        std::vector<unsigned char> bytes;
        size_t size;
    };

    typedef std::array<Cubemap::MipMap, CUBEMAP_NUM_MIPMAPS> MipMapArray_t;

    Cubemap(const std::array<std::shared_ptr<Texture2D>, 6> &textures);
    virtual ~Cubemap();

    inline const std::array<std::shared_ptr<Texture2D>, 6> GetTextures() const
        { return m_textures; }

    void Use();
    void End();

private:
    std::array<std::shared_ptr<Texture2D>, 6> m_textures;
    bool is_created, is_uploaded;

    Cubemap::MipMapArray_t GenerateMipmaps(const std::shared_ptr<Texture2D> &texture);
};

} // namespace apex

#endif
