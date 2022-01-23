#ifndef CUBEMAP_H
#define CUBEMAP_H

#include <array>
#include <vector>
#include <memory>

#include "./texture_2D.h"

#define CUBEMAP_NUM_MIPMAPS 5

namespace hyperion {

class Cubemap : public Texture {
public:

    Cubemap(const std::array<std::shared_ptr<Texture2D>, 6> &textures);
    virtual ~Cubemap();

    inline const std::array<std::shared_ptr<Texture2D>, 6> GetTextures() const
        { return m_textures; }

    virtual void End() override;

protected:
    virtual void Initialize() override;
    virtual void UploadGpuData(bool should_upload_data) override;
    virtual void Use() override;
    virtual void CopyData(Texture * const other) override;

private:
    std::array<std::shared_ptr<Texture2D>, 6> m_textures;
};

} // namespace hyperion

#endif
