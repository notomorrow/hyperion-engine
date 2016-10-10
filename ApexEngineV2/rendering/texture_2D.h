#ifndef TEXTURE_2D_H
#define TEXTURE_2D_H

#include "texture.h"

namespace apex {

class Texture2D : public Texture {
public:
    Texture2D();
    Texture2D(int width, int height, unsigned char *bytes);
    ~Texture2D();

    void Use();
    void End();

private:
    bool is_created, is_uploaded;
};

} // namespace apex

#endif