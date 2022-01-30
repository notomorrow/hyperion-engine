#ifndef TEXTURE_3D_H
#define TEXTURE_3D_H

#include "texture.h"

namespace hyperion {
class Texture3D : public Texture {
public:
    Texture3D();
    Texture3D(int width, int height, int length, unsigned char *bytes);
    virtual ~Texture3D();

    inline int GetLength() const { return m_length; }

    virtual void End() override;
    virtual void CopyData(Texture *const other) override;

protected:
    virtual void UploadGpuData(bool should_upload_data) override;
    virtual void Use() override;

    int m_length;
};
} // namespace hyperion

#endif
