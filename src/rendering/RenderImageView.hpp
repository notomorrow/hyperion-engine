/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>
#include <core/Defines.hpp>

namespace hyperion {
class ImageViewBase : public RenderObject<ImageViewBase>
{
public:
    virtual ~ImageViewBase() override = default;

    HYP_FORCE_INLINE const ImageRef& GetImage() const
    {
        return m_image;
    }

    HYP_API virtual bool IsCreated() const = 0;

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

protected:
    ImageViewBase(
        const ImageRef& image)
        : m_image(image),
          m_mipIndex(0),
          m_numMips(0),
          m_faceIndex(0),
          m_numFaces(0)
    {
    }

    ImageViewBase(
        const ImageRef& image,
        uint32 mipIndex,
        uint32 numMips,
        uint32 faceIndex,
        uint32 numFaces)
        : m_image(image),
          m_mipIndex(mipIndex),
          m_numMips(numMips),
          m_faceIndex(faceIndex),
          m_numFaces(numFaces)
    {
    }

    ImageRef m_image;
    uint32 m_mipIndex;
    uint32 m_numMips;
    uint32 m_faceIndex;
    uint32 m_numFaces;
};

} // namespace hyperion
