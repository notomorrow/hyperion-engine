#ifndef TEXTURE_H
#define TEXTURE_H

#include "../asset/loadable.h"

namespace apex {
class Texture : public Loadable {
    friend class TextureLoader;
public:
    Texture();
    Texture(int width, int height, unsigned char *bytes);
    virtual ~Texture();

    unsigned int GetId() const;

    void SetFormat(int type);
    void SetInternalFormat(int type);
    void SetFilter(int mag, int min);

    static void ActiveTexture(int i);

    virtual void Use() = 0;
    virtual void End() = 0;

protected:
    unsigned int id;
    int ifmt, fmt, width, height;
    unsigned char *bytes;

    int mag_filter, min_filter;
};
}

#endif