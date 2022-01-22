#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "texture_2D.h"

#include <memory>
#include <array>

namespace apex {

class Framebuffer {
public:
    enum FramebufferAttachment {
        FRAMEBUFFER_ATTACHMENT_COLOR = 0,
        FRAMEBUFFER_ATTACHMENT_NORMALS,
        FRAMEBUFFER_ATTACHMENT_POSITIONS,
        FRAMEBUFFER_ATTACHMENT_USERDATA,
        FRAMEBUFFER_ATTACHMENT_SSAO,
        FRAMEBUFFER_ATTACHMENT_DEPTH,
        // --
        FRAMEBUFFER_ATTACHMENT_MAX
    };

    struct FramebufferTextureAttributes {
        const char * const material_key;
        int format;
        int internal_format;
        int min_filter;
        int mag_filter;

        FramebufferTextureAttributes(const char * const material_key, int format, int internal_format, int min_filter, int mag_filter)
            : material_key(material_key),
              format(format),
              internal_format(internal_format),
              min_filter(min_filter),
              mag_filter(mag_filter)
        {
        }
    };

    static const std::array<FramebufferTextureAttributes, FRAMEBUFFER_ATTACHMENT_MAX> default_texture_attributes;

    using FramebufferAttachments_t = std::array<std::shared_ptr<Texture>, FramebufferAttachment::FRAMEBUFFER_ATTACHMENT_MAX>;

    Framebuffer(int width, int height);
    virtual ~Framebuffer();

    inline unsigned int GetId() const { return id; }
    inline int GetWidth() const { return width; }
    inline int GetHeight() const { return height; }

    inline bool HasAttachment(FramebufferAttachment attachment) const { return m_attachments[attachment] != nullptr; }
    inline std::shared_ptr<Texture> &GetAttachment(FramebufferAttachment attachment) { return m_attachments[attachment]; }
    inline const std::shared_ptr<Texture> &GetAttachment(FramebufferAttachment attachment) const { return m_attachments[attachment]; }

    inline FramebufferAttachments_t &GetAttachments() { return m_attachments; }
    inline const FramebufferAttachments_t &GetAttachments() const { return m_attachments; }

    virtual void Store(FramebufferAttachment attachment, std::shared_ptr<Texture> &texture) = 0;

    virtual void Use() = 0;
    void End();

protected:
    unsigned int id;
    int width, height;
    bool is_created, is_uploaded;

    FramebufferAttachments_t m_attachments;
};

} // namespace apex

#endif
