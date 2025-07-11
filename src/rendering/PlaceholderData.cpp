/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/PlaceholderData.hpp>

#include <core/math/Vector2.hpp>

#include <scene/Texture.hpp>

#include <util/img/Bitmap.hpp>

#include <Types.hpp>
#include <Engine.hpp>

namespace hyperion {

template <uint32 NumComponents>
static auto CreatePlaceholderBitmap(Vec2u dimensions)
{
    auto bitmap = Bitmap<NumComponents, ubyte>(dimensions.x, dimensions.y);

    // Set to default color to assist in debugging
    for (uint32 y = 0; y < dimensions.y; y++)
    {
        for (uint32 x = 0; x < dimensions.x; x++)
        {
            bitmap.SetPixel(x, y, { 0xFF, 0x00, 0xFF, 0xFF });
        }
    }

    return bitmap;
}

PlaceholderData::PlaceholderData()
    : m_image_2d_1x1_r8(g_rendering_api->MakeImage(TextureDesc {
          renderer::ImageType::TEXTURE_TYPE_2D,
          renderer::InternalFormat::R8,
          Vec3u::One(),
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
          1,
          ImageFormatCapabilities::SAMPLED })),
      m_image_view_2d_1x1_r8(g_rendering_api->MakeImageView(m_image_2d_1x1_r8)),
      m_image_2d_1x1_r8_storage(g_rendering_api->MakeImage(TextureDesc {
          renderer::ImageType::TEXTURE_TYPE_2D,
          renderer::InternalFormat::R8,
          Vec3u::One(),
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
          1,
          ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED })),
      m_image_view_2d_1x1_r8_storage(g_rendering_api->MakeImageView(m_image_2d_1x1_r8_storage)),
      m_image_3d_1x1x1_r8(g_rendering_api->MakeImage(TextureDesc {
          renderer::ImageType::TEXTURE_TYPE_3D,
          renderer::InternalFormat::R8,
          Vec3u::One(),
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
          1,
          ImageFormatCapabilities::SAMPLED })),
      m_image_view_3d_1x1x1_r8(g_rendering_api->MakeImageView(m_image_3d_1x1x1_r8)),
      m_image_3d_1x1x1_r8_storage(g_rendering_api->MakeImage(TextureDesc {
          renderer::ImageType::TEXTURE_TYPE_3D,
          renderer::InternalFormat::R8,
          Vec3u::One(),
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
          1,
          ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED })),
      m_image_view_3d_1x1x1_r8_storage(g_rendering_api->MakeImageView(m_image_3d_1x1x1_r8_storage)),
      m_image_cube_1x1_r8(g_rendering_api->MakeImage(TextureDesc {
          renderer::ImageType::TEXTURE_TYPE_CUBEMAP,
          renderer::InternalFormat::R8,
          Vec3u::One(),
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
          1,
          ImageFormatCapabilities::SAMPLED })),
      m_image_view_cube_1x1_r8(g_rendering_api->MakeImageView(m_image_cube_1x1_r8)),
      m_image_2d_1x1_r8_array(g_rendering_api->MakeImage(TextureDesc {
          renderer::ImageType::TEXTURE_TYPE_2D_ARRAY,
          renderer::InternalFormat::R8,
          Vec3u::One(),
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
          1,
          ImageFormatCapabilities::SAMPLED })),
      m_image_view_2d_1x1_r8_array(g_rendering_api->MakeImageView(m_image_2d_1x1_r8_array)),
      m_image_cube_1x1_r8_array(g_rendering_api->MakeImage(TextureDesc {
          renderer::ImageType::TEXTURE_TYPE_CUBEMAP_ARRAY,
          renderer::InternalFormat::R8,
          Vec3u::One(),
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
          1,
          ImageFormatCapabilities::SAMPLED })),
      m_image_view_cube_1x1_r8_array(g_rendering_api->MakeImageView(m_image_cube_1x1_r8_array)),
      m_sampler_linear(g_rendering_api->MakeSampler(
          renderer::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::WrapMode::TEXTURE_WRAP_REPEAT)),
      m_sampler_linear_mipmap(g_rendering_api->MakeSampler(
          renderer::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
          renderer::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::WrapMode::TEXTURE_WRAP_REPEAT)),
      m_sampler_nearest(g_rendering_api->MakeSampler(
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE))
{
}

PlaceholderData::~PlaceholderData()
{
    DebugLog(LogType::Debug, "PlaceholderData destructor\n");
}

void PlaceholderData::Create()
{
#pragma region Image and ImageView
    // These will soon be deprecated (except the samplers) - we will instead use Texture instead of individual image/image view
    m_image_2d_1x1_r8->SetDebugName(NAME("Placeholder_2D_1x1_R8"));
    DeferCreate(m_image_2d_1x1_r8);

    m_image_view_2d_1x1_r8->SetDebugName(NAME("Placeholder_2D_1x1_R8_View"));
    DeferCreate(m_image_view_2d_1x1_r8);

    m_image_2d_1x1_r8_storage->SetDebugName(NAME("Placeholder_2D_1x1_R8_Storage"));
    DeferCreate(m_image_2d_1x1_r8_storage);

    m_image_view_2d_1x1_r8_storage->SetDebugName(NAME("Placeholder_2D_1x1_R8_Storage_View"));
    DeferCreate(m_image_view_2d_1x1_r8_storage);

    m_image_3d_1x1x1_r8->SetDebugName(NAME("Placeholder_3D_1x1x1_R8"));
    DeferCreate(m_image_3d_1x1x1_r8);

    m_image_view_3d_1x1x1_r8->SetDebugName(NAME("Placeholder_3D_1x1x1_R8_View"));
    DeferCreate(m_image_view_3d_1x1x1_r8);

    m_image_3d_1x1x1_r8_storage->SetDebugName(NAME("Placeholder_3D_1x1x1_R8_Storage"));
    DeferCreate(m_image_3d_1x1x1_r8_storage);

    m_image_view_3d_1x1x1_r8_storage->SetDebugName(NAME("Placeholder_3D_1x1x1_R8_Storage_View"));
    DeferCreate(m_image_view_3d_1x1x1_r8_storage);

    m_image_cube_1x1_r8->SetDebugName(NAME("Placeholder_Cube_1x1_R8"));
    DeferCreate(m_image_cube_1x1_r8);

    m_image_view_cube_1x1_r8->SetDebugName(NAME("Placeholder_Cube_1x1_R8_View"));
    DeferCreate(m_image_view_cube_1x1_r8);

    m_image_2d_1x1_r8_array->SetDebugName(NAME("Placeholder_2D_1x1_R8_Array"));
    DeferCreate(m_image_2d_1x1_r8_array);

    m_image_view_2d_1x1_r8_array->SetDebugName(NAME("Placeholder_2D_1x1_R8_Array_View"));
    DeferCreate(m_image_view_2d_1x1_r8_array);

    m_image_cube_1x1_r8_array->SetDebugName(NAME("Placeholder_Cube_1x1_R8_Array"));
    DeferCreate(m_image_cube_1x1_r8_array);

    m_image_view_cube_1x1_r8_array->SetDebugName(NAME("Placeholder_Cube_1x1_R8_Array_View"));
    DeferCreate(m_image_view_cube_1x1_r8_array);

#pragma endregion Image and ImageView

#pragma region Textures

    DefaultTexture2D = CreateObject<Texture>(TextureData {
        TextureDesc {
            ImageType::TEXTURE_TYPE_2D,
            InternalFormat::RGBA8,
            Vec3u::One(),
            FilterMode::TEXTURE_FILTER_NEAREST,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            1,
            ImageFormatCapabilities::SAMPLED | ImageFormatCapabilities::STORAGE },
        CreatePlaceholderBitmap<4>(Vec2u::One()).GetUnpackedBytes(4) });

    DefaultTexture2D->SetName(NAME("Placeholder_Texture_2D_1x1_R8"));
    InitObject(DefaultTexture2D);
    DefaultTexture2D->SetPersistentRenderResourceEnabled(true);

    DefaultTexture3D = CreateObject<Texture>(TextureData {
        TextureDesc {
            ImageType::TEXTURE_TYPE_3D,
            InternalFormat::R8,
            Vec3u::One(),
            FilterMode::TEXTURE_FILTER_NEAREST,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            1,
            ImageFormatCapabilities::SAMPLED | ImageFormatCapabilities::STORAGE } });

    DefaultTexture3D->SetName(NAME("Placeholder_Texture_3D_1x1x1_R8"));
    InitObject(DefaultTexture3D);
    DefaultTexture3D->SetPersistentRenderResourceEnabled(true);

    DefaultCubemap = CreateObject<Texture>(TextureData {
        TextureDesc {
            ImageType::TEXTURE_TYPE_CUBEMAP,
            InternalFormat::R8,
            Vec3u::One(),
            FilterMode::TEXTURE_FILTER_NEAREST,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            1,
            ImageFormatCapabilities::SAMPLED | ImageFormatCapabilities::STORAGE } });

    DefaultCubemap->SetName(NAME("Placeholder_Texture_Cube_1x1_R8"));
    InitObject(DefaultCubemap);
    DefaultCubemap->SetPersistentRenderResourceEnabled(true);

    DefaultTexture2DArray = CreateObject<Texture>(TextureData {
        TextureDesc {
            ImageType::TEXTURE_TYPE_2D_ARRAY,
            InternalFormat::R8,
            Vec3u::One(),
            FilterMode::TEXTURE_FILTER_NEAREST,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            1,
            ImageFormatCapabilities::SAMPLED | ImageFormatCapabilities::STORAGE } });

    DefaultTexture2DArray->SetName(NAME("Placeholder_Texture_2D_1x1_R8_Array"));
    InitObject(DefaultTexture2DArray);
    DefaultTexture2DArray->SetPersistentRenderResourceEnabled(true);

    DefaultCubemapArray = CreateObject<Texture>(TextureData {
        TextureDesc {
            ImageType::TEXTURE_TYPE_CUBEMAP_ARRAY,
            InternalFormat::R8,
            Vec3u::One(),
            FilterMode::TEXTURE_FILTER_NEAREST,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            1,
            ImageFormatCapabilities::SAMPLED | ImageFormatCapabilities::STORAGE } });

    DefaultCubemapArray->SetName(NAME("Placeholder_Texture_Cube_1x1_R8_Array"));
    InitObject(DefaultCubemapArray);
    DefaultCubemapArray->SetPersistentRenderResourceEnabled(true);

#pragma endregion Textures

#pragma region Samplers

    m_sampler_linear->SetDebugName(NAME("Placeholder_Sampler_Linear"));
    DeferCreate(m_sampler_linear);

    m_sampler_linear_mipmap->SetDebugName(NAME("Placeholder_Sampler_Linear_Mipmap"));
    DeferCreate(m_sampler_linear_mipmap);

    m_sampler_nearest->SetDebugName(NAME("Placeholder_Sampler_Nearest"));
    DeferCreate(m_sampler_nearest);

#pragma endregion Samplers
}

void PlaceholderData::Destroy()
{
    SafeRelease(std::move(m_image_2d_1x1_r8));
    SafeRelease(std::move(m_image_view_2d_1x1_r8));
    SafeRelease(std::move(m_image_2d_1x1_r8_storage));
    SafeRelease(std::move(m_image_view_2d_1x1_r8_storage));
    SafeRelease(std::move(m_image_3d_1x1x1_r8));
    SafeRelease(std::move(m_image_view_3d_1x1x1_r8));
    SafeRelease(std::move(m_image_3d_1x1x1_r8_storage));
    SafeRelease(std::move(m_image_view_3d_1x1x1_r8_storage));
    SafeRelease(std::move(m_image_cube_1x1_r8));
    SafeRelease(std::move(m_image_view_cube_1x1_r8));
    SafeRelease(std::move(m_image_2d_1x1_r8_array));
    SafeRelease(std::move(m_image_view_2d_1x1_r8_array));
    SafeRelease(std::move(m_image_cube_1x1_r8_array));
    SafeRelease(std::move(m_image_view_cube_1x1_r8_array));
    SafeRelease(std::move(m_sampler_linear));
    SafeRelease(std::move(m_sampler_linear_mipmap));
    SafeRelease(std::move(m_sampler_nearest));

    for (auto& buffer_map : m_buffers)
    {
        for (auto& it : buffer_map.second)
        {
            SafeRelease(std::move(it.second));
        }
    }

    m_buffers.Clear();
}

GPUBufferRef PlaceholderData::CreateGPUBuffer(GPUBufferType buffer_type, SizeType size)
{
    GPUBufferRef gpu_buffer = g_rendering_api->MakeGPUBuffer(buffer_type, size);
    HYPERION_ASSERT_RESULT(gpu_buffer->Create());

    return gpu_buffer;
}

} // namespace hyperion
