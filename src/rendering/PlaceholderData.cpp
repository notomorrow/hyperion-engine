#include "PlaceholderData.hpp"

#include <Engine.hpp>

namespace hyperion::v2 {
PlaceholderData::PlaceholderData()
    : m_image_2d_1x1_r8(
          renderer::Extent2D(1, 1),
          renderer::Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8,
          renderer::Image::FilterMode::TEXTURE_FILTER_NEAREST,
          nullptr
      ),
      m_image_cube_1x1_r8(
          renderer::Extent2D(1, 1),
          renderer::Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8,
          renderer::Image::FilterMode::TEXTURE_FILTER_NEAREST,
          nullptr
      ),
      m_sampler_linear(
          renderer::Image::FilterMode::TEXTURE_FILTER_LINEAR,
          renderer::Image::WrapMode::TEXTURE_WRAP_REPEAT
      ),
      m_sampler_nearest(
          renderer::Image::FilterMode::TEXTURE_FILTER_NEAREST,
          renderer::Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
      )
{
}

void PlaceholderData::Create(Engine *engine)
{
    auto *device = engine->GetDevice();

    HYPERION_ASSERT_RESULT(m_image_2d_1x1_r8.Create(device));
    HYPERION_ASSERT_RESULT(m_image_view_2d_1x1_r8.Create(device, &m_image_2d_1x1_r8));
    HYPERION_ASSERT_RESULT(m_image_cube_1x1_r8.Create(device));
    HYPERION_ASSERT_RESULT(m_image_view_cube_1x1_r8.Create(device, &m_image_cube_1x1_r8));
    HYPERION_ASSERT_RESULT(m_sampler_linear.Create(device));
    HYPERION_ASSERT_RESULT(m_sampler_nearest.Create(device));
}

void PlaceholderData::Destroy(Engine *engine)
{
    auto *device = engine->GetDevice();

    HYPERION_ASSERT_RESULT(m_image_2d_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_view_2d_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_cube_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_image_view_cube_1x1_r8.Destroy(device));
    HYPERION_ASSERT_RESULT(m_sampler_linear.Destroy(device));
    HYPERION_ASSERT_RESULT(m_sampler_nearest.Destroy(device));
}

} // namespace hyperion::v2
