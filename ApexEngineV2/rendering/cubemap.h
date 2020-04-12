#ifndef CUBEMAP_H
#define CUBEMAP_H

#include <array>
#include <memory>

#include "./texture_2D.h"

namespace apex {

class Cubemap : public Texture {
public:
    Cubemap(const std::array<std::shared_ptr<Texture2D>, 6> &textures);
    virtual ~Cubemap();

    void Use();
    void End();

private:
    std::array<std::shared_ptr<Texture2D>, 6> m_textures;
    bool is_created, is_uploaded;
};

} // namespace apex

#endif
