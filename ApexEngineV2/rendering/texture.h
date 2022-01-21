#ifndef TEXTURE_H
#define TEXTURE_H

#include "../asset/loadable.h"

namespace apex {

class Texture : public Loadable {
    friend class TextureLoader;
public:
    enum TextureType {
        TEXTURE_TYPE_2D = 0x0,
        TEXTURE_TYPE_3D = 0x1
    };

    Texture(TextureType texture_type);
    Texture(TextureType texture_type, int width, int height, unsigned char *bytes);
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

    inline TextureType GetTextureType() const { return m_texture_type; }

    unsigned char * const GetBytes() const { return bytes; }

    static void ActiveTexture(int i);

    void Prepare();

    virtual void End() = 0;

    // TEMP: remove once all uniforms are bound directly to texture id, and Begin() calls replaced w/ Prepare()
    inline void Begin()
    {
        Prepare(); 
        Use();
    }

protected:
    unsigned int id;
    int ifmt, fmt, width, height;
    unsigned char *bytes;

    int mag_filter, min_filter;
    int wrap_s, wrap_t;

    TextureType m_texture_type;

    virtual void Initialize();
    virtual void Deinitialize();
    virtual void UploadGpuData() = 0;
    virtual void Use() = 0;

private:
    bool is_created, is_uploaded;
};

} // namespace apex

#endif
