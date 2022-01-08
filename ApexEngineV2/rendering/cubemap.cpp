#include "./cubemap.h"
#include "../util.h"
#include "../math/math_util.h"

#include <iostream>

namespace apex {

Cubemap::Cubemap(const std::array<std::shared_ptr<Texture2D>, 6> &textures)
    : Texture(),
      m_textures(textures)
{
    is_uploaded = false;
    is_created = false;
}

Cubemap::~Cubemap()
{
    if (is_created) {
        glDeleteTextures(1, &id);
    }

    is_uploaded = false;
    is_created = false;
}

void Cubemap::Use()
{
    if (!is_created) {
        glGenTextures(1, &id);
        CatchGLErrors("Failed to generate cubemap texture", false);

        glEnable(GL_TEXTURE_CUBE_MAP);
        CatchGLErrors("Failed to enable GL_TEXTURE_CUBE_MAP", false);
        //glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        is_created = true;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, id);

    if (!is_uploaded) {
        //Our image for the mipmap with pair level and red color
        unsigned char imageArray2[1024*1024*3];
        for(int i = 0; i < 1024*1024*3; i++){
            if(i%3 == 0)
                imageArray2[i] = 255;
            else
                imageArray2[i] = 0;
        }

        for (size_t i = 0; i < m_textures.size(); i++) {
            const auto &tex = m_textures[i];

            if (tex == nullptr) {
                throw std::runtime_error("Could not upload cubemap because texture #" + std::to_string(i + 1) + " was nullptr.");
            } else if (tex->GetBytes() == nullptr) {
                throw std::runtime_error("Could not upload cubemap because texture #" + std::to_string(i + 1) + " had no bytes set.");
            }

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, tex->GetInternalFormat(),
                tex->GetWidth(), tex->GetHeight(), 0, tex->GetFormat(), GL_UNSIGNED_BYTE, tex->GetBytes());

            const auto mipmap_array = GenerateMipmaps(tex);
            
            for (unsigned int j = 0; j < mipmap_array.size(); j++) {
                const auto it = mipmap_array[j];
    
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, j + 1, GL_RGB8,
                    it.size, it.size, 0, GL_RGB, GL_UNSIGNED_BYTE, it.bytes.data());
            }

            
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, CUBEMAP_NUM_MIPMAPS);

        // glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        CatchGLErrors("Failed to upload cubemap");

        is_uploaded = true;
    }

}

void Cubemap::End()
{
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

static void BlurComponent(const unsigned char *src, unsigned char *dst, int width, int height, double radius)
{
    const int rs = MathUtil::Ceil(radius * 2.57);

    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            double val = 0.0,
                weight_sum = 0.0;

            for (int iy = i - rs; iy < i + rs + 1; iy++) {
                for (int ix = j - rs; ix < j + rs + 1; ix++) {
                    int x = MathUtil::Min(width - 1, MathUtil::Max(0, ix));
                    int y = MathUtil::Min(height - 1, MathUtil::Max(0, iy));
                    double dsq = (ix - j) * (ix - j) + (iy - i) * (iy - i);
                    double weight = MathUtil::Exp(-double(dsq) / (2.0 * radius * radius)) / (MathUtil::PI * 2.0 * radius * radius);
                    val += double(src[y * width + x]) * weight;
                    weight_sum += weight;
                }
            }

            int value = MathUtil::Round(val / weight_sum);

            dst[i * width + j] = value;
        }
    }
}

static void BlurImage(const unsigned char *src, unsigned char *dst, int width, int height, double radius)
{
    unsigned char *reds = new unsigned char[width * height];
    unsigned char *dst_reds = new unsigned char[width * height];
    unsigned char *greens = new unsigned char[width * height];
    unsigned char *dst_greens = new unsigned char[width * height];
    unsigned char *blues = new unsigned char[width * height];
    unsigned char *dst_blues = new unsigned char[width * height];

    size_t offset = 0;

    for (size_t i = 0; i < width * height * 3; i += 3) {
        reds[offset] = src[i];
        greens[offset] = src[i + 1];
        blues[offset] = src[i + 2];
        offset++;
    }

    BlurComponent(reds, dst_reds, width, height, radius);
    BlurComponent(greens, dst_greens, width, height, radius);
    BlurComponent(blues, dst_blues, width, height, radius);

    for (size_t i = 0; i < width * height; i++) {
        *dst++ = reds[i];
        *dst++ = greens[i];
        *dst++ = blues[i];
    }

    delete[] reds;
    delete[] dst_reds;
    delete[] greens;
    delete[] dst_greens;
    delete[] blues;
    delete[] dst_blues;
}

static void ResizeImageBilinear(const unsigned char *src, int srcWidth, int srcHeight, unsigned char *dst, int dstWidth, int dstHeight, int numComponents) {
    int srcPitch = srcWidth * numComponents;

    float ratioX = (float)srcWidth / dstWidth;
    float ratioY = (float)srcHeight / dstHeight;

    for (int y = 0; y < dstHeight; y++) {
        float fY0 = y * ratioY;
        float fracY = MathUtil::Fract(fY0);

        int iY0 = fY0 - fracY;
        int iY1 = MathUtil::Min(iY0 + 1, srcHeight - 1);

        int offsetY0 = iY0 * srcPitch;
        int offsetY1 = iY1 * srcPitch;

        for (int x = 0; x < dstWidth; x++) {
            float fX0 = x * ratioX;
            float fracX = MathUtil::Fract(fX0);

            int iX0 = fX0 - fracX;
            int iX1 = MathUtil::Min(iX0 + 1, srcWidth - 1);

            int offsetX0 = iX0 * numComponents;
            int offsetX1 = iX1 * numComponents;

            const unsigned char *srcPtrY[2];
            srcPtrY[0] = &src[offsetY0];
            srcPtrY[1] = &src[offsetY1];

            for (int i = 0; i < numComponents; i++) {
                int index0 = offsetX0 + i;
                int index1 = offsetX1 + i;

                // NOTE: Should we need to lerp in linear color space ?
                float p0 = MathUtil::Lerp<float>(srcPtrY[0][index0], srcPtrY[0][index1], fracX);
                float p1 = MathUtil::Lerp<float>(srcPtrY[1][index0], srcPtrY[1][index1], fracX);
                float po = MathUtil::Lerp<float>(p0, p1, fracY);

                *dst++ = (unsigned char)po;
            }
        }
    }
} 


Cubemap::MipMapArray_t Cubemap::GenerateMipmaps(const std::shared_ptr<Texture2D> &texture)
{
    // size of texture must be power of 2.
    Cubemap::MipMapArray_t mipmap_array;

    for (unsigned int i = 0; i < mipmap_array.size(); i++) {

        const unsigned int image_size = texture->GetWidth() >> (i + 1),
            prev_image_size = texture->GetWidth() >> i;

        mipmap_array[i].bytes.resize(image_size * image_size * 3);
        mipmap_array[i].size = image_size;

        unsigned char *image_bytes = mipmap_array[i].bytes.data();
        unsigned char *previous_image_bytes;

        if (i == 0) {
            previous_image_bytes = texture->GetBytes();
        } else {
            previous_image_bytes = mipmap_array[i - 1].bytes.data();
        }

        ResizeImageBilinear(previous_image_bytes, prev_image_size, prev_image_size, image_bytes, image_size, image_size, 3);
        
        // unsigned char *copy = new unsigned char[image_size * image_size * 3];

        // std::memcpy(copy, image_bytes, image_size * image_size * 3);

        // BlurImage(copy, image_bytes, image_size, image_size, 1.0);

        // delete[] copy;
    }

    return mipmap_array;
}

} // namespace apex
