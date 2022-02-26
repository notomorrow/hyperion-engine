#ifndef TEXTURE_H
#define TEXTURE_H

#include "../asset/loadable.h"

namespace hyperion {

class Texture : public Loadable {
    friend class TextureLoader;
public:
    enum TextureType {
        TEXTURE_TYPE_2D = 0,
        TEXTURE_TYPE_3D = 1,
        TEXTURE_TYPE_CUBEMAP = 2
    };

    enum class TextureBaseFormat {
        TEXTURE_FORMAT_NONE,
        TEXTURE_FORMAT_R,
        TEXTURE_FORMAT_RG,
        TEXTURE_FORMAT_RGB,
        TEXTURE_FORMAT_RGBA,

        TEXTURE_FORMAT_DEPTH
    };

    enum class TextureInternalFormat {
        TEXTURE_INTERNAL_FORMAT_NONE,
        TEXTURE_INTERNAL_FORMAT_R8,
        TEXTURE_INTERNAL_FORMAT_RG8,
        TEXTURE_INTERNAL_FORMAT_RGB8,
        TEXTURE_INTERNAL_FORMAT_RGBA8,
        TEXTURE_INTERNAL_FORMAT_R16,
        TEXTURE_INTERNAL_FORMAT_RG16,
        TEXTURE_INTERNAL_FORMAT_RGB16,
        TEXTURE_INTERNAL_FORMAT_RGBA16,
        TEXTURE_INTERNAL_FORMAT_R32,
        TEXTURE_INTERNAL_FORMAT_RG32,
        TEXTURE_INTERNAL_FORMAT_RGB32,
        TEXTURE_INTERNAL_FORMAT_RGBA32,
        TEXTURE_INTERNAL_FORMAT_R16F,
        TEXTURE_INTERNAL_FORMAT_RG16F,
        TEXTURE_INTERNAL_FORMAT_RGB16F,
        TEXTURE_INTERNAL_FORMAT_RGBA16F,
        TEXTURE_INTERNAL_FORMAT_R32F,
        TEXTURE_INTERNAL_FORMAT_RG32F,
        TEXTURE_INTERNAL_FORMAT_RGB32F,
        TEXTURE_INTERNAL_FORMAT_RGBA32F,

        TEXTURE_INTERNAL_FORMAT_DEPTH_16,
        TEXTURE_INTERNAL_FORMAT_DEPTH_24,
        TEXTURE_INTERNAL_FORMAT_DEPTH_32,
        TEXTURE_INTERNAL_FORMAT_DEPTH_32F
    };

    Texture(TextureType texture_type);
    Texture(TextureType texture_type, int width, int height, unsigned char *bytes);
    Texture(const Texture &other) = delete;
    Texture &operator=(const Texture &other) = delete;
    virtual ~Texture();

    unsigned int GetId() const;

    inline bool IsCreated() const { return is_created; }
    inline bool IsUploaded() const { return is_uploaded; }

    void SetFormat(TextureBaseFormat type);
    inline TextureBaseFormat GetFormat() const { return fmt; }
    void SetInternalFormat(TextureInternalFormat type);
    inline TextureInternalFormat GetInternalFormat() const { return ifmt; }

    inline int GetWidth() const { return width; }
    inline int GetHeight() const { return height; }

    void SetFilter(int mag, int min);
    inline int GetMagFilter() const { return mag_filter; }
    inline int GetMinFilter() const { return min_filter; }

    void SetWrapMode(int s, int t);
    inline int GetWrapS() const { return wrap_s; }
    inline int GetWrapT() const { return wrap_t; }

    virtual void CopyData(Texture * const other) = 0;

    inline TextureType GetTextureType() const { return m_texture_type; }

    unsigned char * const GetBytes() const { return bytes; }

    static size_t NumComponents(TextureBaseFormat format);
    static void ActiveTexture(int i);

    void Prepare(bool should_upload_data = true);

    virtual void End() = 0;

    inline void Begin(bool should_upload_data = true)
    {
        Prepare(should_upload_data); 
        Use();
    }

protected:
    unsigned int id;

    int width, height;
    TextureInternalFormat ifmt;
    TextureBaseFormat fmt;
    unsigned char *bytes;

    int mag_filter, min_filter;
    int wrap_s, wrap_t;

    bool m_is_storage;
    bool is_created, is_uploaded;

    TextureType m_texture_type;

    virtual void Initialize();
    virtual void Deinitialize();
    virtual void UploadGpuData(bool should_upload_data) = 0;
    virtual void Use() = 0;

    // temp until vulkan is main renderer
    static int ToOpenGLInternalFormat(TextureInternalFormat);
    static int ToOpenGLBaseFormat(TextureBaseFormat);

private:
};

} // namespace hyperion

#endif
