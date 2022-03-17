#ifndef CALCULATE_SPHERICAL_HARMONICS_H
#define CALCULATE_SPHERICAL_HARMONICS_H

#include "../../../math/vector2.h"
#include "../../../math/vector3.h"
#include "../../cubemap.h"

#include <array>
#include <thread>
#include <mutex>

namespace hyperion {

static inline Vector3 MapXYSToDirection(int x, int y, int s, int width, int height)
{
    float u = ((float(x) + 0.5f) / float(width)) * 2.0f - 1.0f;
    float v = ((float(y) + 0.5f) / float(height)) * 2.0f - 1.0f;
    v *= -1.0;

    Vector3 dir;

    // +x, -x, +y, -y, +z, -z
    switch (s) {
    case 0:
        dir = Vector3(1.0, v, -u);
        break;
    case 1:
        dir = Vector3(-1.0, v, u);
        break;
    case 2:
        dir = Vector3(u, 1.0, -v);
        break;
    case 3:
        dir = Vector3(u, -1.0, v);
        break;
    case 4:
        dir = Vector3(u, v, 1.0);
        break;
    case 5:
        dir = Vector3(-u, v, -1.0);
        break;
    default:
        break;
    }

    return dir.Normalize();
}

static inline std::array<float, 9> ProjectOntoSH9(const Vector3 &dir)
{
    std::array<float, 9> sh;

    // Band 0
    sh[0] = 0.282095f;

    // Band 1
    sh[1] = 0.488603f * dir.y;
    sh[2] = 0.488603f * dir.z;
    sh[3] = 0.488603f * dir.x;

    // Band 2
    sh[4] = 1.092548f * dir.x * dir.y;
    sh[5] = 1.092548f * dir.y * dir.z;
    sh[6] = 0.315392f * (3.0f * dir.z * dir.z - 1.0f);
    sh[7] = 1.092548f * dir.x * dir.z;
    sh[8] = 0.546274f * (dir.x * dir.x - dir.y * dir.y);

    return sh;
}

static inline std::array<Vector3, 9> ProjectOntoSH9Color(const Vector3 &dir, const Vector3 &color)
{
    auto sh = ProjectOntoSH9(dir);
    std::array<Vector3, 9> shColor;

    for (int i = 0; i < 9; i++) {
        shColor[i] = color * sh[i];
    }

    return shColor;
}

static inline std::array<Vector3, 9> CalculateSphericalHarmonics(Cubemap *cubemap)
{
    AssertThrowMsg(
        cubemap->GetInternalFormat() == Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8
            || cubemap->GetInternalFormat() == Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8,
        "Cubemap expected to be in rgb8 or rgba8 format"
    );

    if (!cubemap->IsUploaded()) {
        return {}; // TODO: error result
    }

    const auto &textures = cubemap->GetTextures();

    for (auto &texture : textures) {
        if (texture->GetBytes() == nullptr) {
            return {}; // TODO: error result
        }
    }

    float weight_sum = 0.0;

    std::array<Vector3, 9> result{ Vector3::Zero() };

    std::mutex sum_mtx;
    std::vector<std::thread> threads;

    for (int face = 0; face < 6; face++) {
        const auto &texture = cubemap->GetTextures()[face];

        unsigned char *texture_bytes = (unsigned char *)malloc(texture->GetWidth() * texture->GetHeight() * Texture::NumComponents(texture->GetFormat()));
        std::memcpy(texture_bytes, texture->GetBytes(), texture->GetWidth() * texture->GetHeight() * Texture::NumComponents(texture->GetFormat()));

        auto new_texture = std::make_shared<Texture2D>(texture->GetWidth(), texture->GetHeight(), texture_bytes);

        threads.emplace_back([texture, new_texture, face, &sum_mtx, &weight_sum, &result]() {
            new_texture->SetFormat(texture->GetFormat());
            new_texture->SetInternalFormat(texture->GetInternalFormat());
            new_texture->SetFilter(Texture::TextureFilterMode::TEXTURE_FILTER_NEAREST);
            new_texture->Resize(64, 64);

            for (int v = 0; v < new_texture->GetWidth(); v++) {
                for (int u = 0; u < new_texture->GetHeight(); u++) {
                    Vector2 xy((float(u) + 0.5) / new_texture->GetWidth(), (float(v) + 0.5) / new_texture->GetHeight());
                    xy *= Vector2(2.0);
                    xy -= Vector2(1.0);

                    float temp = 1.0 + xy.x * xy.x + xy.y * xy.y;
                    float weight = 4.0 / (sqrt(temp) * temp);

                    Vector3 dir = MapXYSToDirection(u, v, face, new_texture->GetWidth(), new_texture->GetHeight());

                    Vector3 pixel_rgb;
                    pixel_rgb.x = new_texture->GetBytes()[((v * new_texture->GetWidth() + u) * Texture::NumComponents(new_texture->GetFormat()))];
                    pixel_rgb.y = new_texture->GetBytes()[((v * new_texture->GetWidth() + u) * Texture::NumComponents(new_texture->GetFormat())) + 1];
                    pixel_rgb.z = new_texture->GetBytes()[((v * new_texture->GetWidth() + u) * Texture::NumComponents(new_texture->GetFormat())) + 2];
                    pixel_rgb /= 255.0f;

                    std::array<Vector3, 9> sh = ProjectOntoSH9Color(dir, pixel_rgb);

                    sum_mtx.lock();

                    for (int i = 0; i < 9; i++) {
                        result[i] += sh[i] * weight;
                    }

                    weight_sum += weight;

                    sum_mtx.unlock();
                }
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    std::array<Vector3, 9> sh_samples;

    for (int i = 0; i < 9; i++) {
        sh_samples[i] = result[i] * (4.0f * 3.14159f) / MathUtil::Max(weight_sum, 0.0001f);
    }

    return sh_samples;
}

} // namespace hyperion

#endif