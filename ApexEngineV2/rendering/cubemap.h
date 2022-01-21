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

    virtual void End() override;

protected:
    virtual void Initialize() override;
    virtual void UploadGpuData() override;
    virtual void Use() override;

private:
    std::array<std::shared_ptr<Texture2D>, 6> m_textures;
};

} // namespace apex

#endif
