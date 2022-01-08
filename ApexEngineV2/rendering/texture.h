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
    inline int GetFormat() const { return fmt; }
    void SetInternalFormat(int type);
    inline int GetInternalFormat() const { return ifmt; }
    void SetFilter(int mag, int min);
    void SetWrapMode(int s, int t);

    inline int GetWidth() const { return width; }
    inline int GetHeight() const { return height; }

    unsigned char * const GetBytes() const { return bytes; }

    static void ActiveTexture(int i);

    virtual void Use() = 0;
    virtual void End() = 0;

protected:
    unsigned int id;
    int ifmt, fmt, width, height;
    unsigned char *bytes;

    int mag_filter, min_filter;
    int wrap_s, wrap_t;
};

} // namespace apex

#endif
