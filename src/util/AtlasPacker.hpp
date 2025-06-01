/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ATLAS_PACKER_HPP
#define HYPERION_ATLAS_PACKER_HPP

#include <core/math/Vector2.hpp>
#include <core/math/Vector4.hpp>

#include <core/containers/Array.hpp>

#include <Types.hpp>

#include <algorithm>

namespace hyperion {

template <class AtlasElement>
struct AtlasPacker
{
    Vec2u atlas_dimensions;
    Array<AtlasElement, DynamicAllocator> elements;
    Array<Pair<Vec2i, Vec2i>> free_spaces;

    AtlasPacker()
        : atlas_dimensions(Vec2u::One())
    {
    }

    AtlasPacker(const Vec2u& atlas_dimensions);

    AtlasPacker(const AtlasPacker<AtlasElement>& other) = default;
    AtlasPacker<AtlasElement>& operator=(const AtlasPacker<AtlasElement>& other) = default;

    AtlasPacker(AtlasPacker<AtlasElement>&& other) = default;
    AtlasPacker<AtlasElement>& operator=(AtlasPacker<AtlasElement>&& other) = default;

    ~AtlasPacker() = default;

    bool AddElement(const Vec2u& element_dimensions, AtlasElement& out_element);
    bool RemoveElement(const AtlasElement& element);

    void Clear();

    bool CalculateFitOffset(uint32 index, const Vec2u& dimensions, Vec2u& out_offset) const;
    void AddSkylineNode(uint32 before_index, const Vec2u& dimensions, const Vec2u& offset);
    void MergeSkyline();
};

template <class AtlasElement>
AtlasPacker<AtlasElement>::AtlasPacker(const Vec2u& atlas_dimensions)
    : atlas_dimensions(atlas_dimensions)
{
    free_spaces.EmplaceBack(Vec2i::Zero(), Vec2i { int(atlas_dimensions.x), 0 });
}

template <class AtlasElement>
bool AtlasPacker<AtlasElement>::AddElement(const Vec2u& element_dimensions, AtlasElement& out_element)
{
    int best_y = INT32_MAX;
    int best_x = -1;
    int best_index = -1;

    for (SizeType i = 0; i < free_spaces.Size(); i++)
    {
        Vec2u offset;

        if (CalculateFitOffset(uint32(i), element_dimensions, offset))
        {
            if (int(offset.y) < best_y)
            {
                best_x = int(offset.x);
                best_y = int(offset.y);
                best_index = int(i);
            }
        }
    }

    if (best_index != -1)
    {
        out_element.offset_coords = Vec2u { uint32(best_x), uint32(best_y) };
        out_element.offset_uv = Vec2f(out_element.offset_coords) / Vec2f(atlas_dimensions);
        out_element.dimensions = element_dimensions;
        out_element.scale = Vec2f(element_dimensions) / Vec2f(atlas_dimensions);

        AddSkylineNode(uint32(best_index), Vec2u { element_dimensions.x, 0 }, Vec2u { uint32(best_x), uint32(best_y) + element_dimensions.y });

        return true;
    }

    return false;
}

template <class AtlasElement>
bool AtlasPacker<AtlasElement>::RemoveElement(const AtlasElement& element)
{
    auto it = elements.Find(element);

    if (it == elements.End())
    {
        return false;
    }

    free_spaces.EmplaceBack(Vec2i(element.offset_uv * Vec2f(atlas_dimensions)), Vec2i(element.dimensions));

    std::sort(free_spaces.Begin(), free_spaces.End(), [](const auto& a, const auto& b)
        {
            return a.first.x < b.first.x;
        });

    MergeSkyline();

    elements.Erase(it);

    return true;
}

template <class AtlasElement>
void AtlasPacker<AtlasElement>::Clear()
{
    free_spaces.Clear();
    elements.Clear();

    // Add the initial skyline node
    free_spaces.EmplaceBack(Vec2i::Zero(), Vec2i { int(atlas_dimensions.x), 0 });
}

// Based on: https://jvernay.fr/en/blog/skyline-2d-packer/implementation/
template <class AtlasElement>
bool AtlasPacker<AtlasElement>::CalculateFitOffset(uint32 index, const Vec2u& dimensions, Vec2u& out_offset) const
{
    auto& space_offset = free_spaces[index].first;
    auto& space_dimensions = free_spaces[index].second;

    const int x = space_offset.x;
    int y = space_offset.y;

    int remaining_width = space_dimensions.x;

    if (x + dimensions.x > atlas_dimensions.x)
    {
        return false;
    }

    for (uint32 i = index; i < free_spaces.Size() && remaining_width > 0; i++)
    {
        const int node_y = space_offset.y;

        y = MathUtil::Max(y, node_y);

        if (y + dimensions.y > atlas_dimensions.y)
        {
            return false;
        }

        remaining_width -= space_dimensions.x;
    }

    out_offset = Vec2u { uint32(x), uint32(y) };

    return true;
}

template <class AtlasElement>
void AtlasPacker<AtlasElement>::AddSkylineNode(uint32 before_index, const Vec2u& dimensions, const Vec2u& offset)
{
    free_spaces.Insert(free_spaces.Begin() + before_index, { Vec2i(offset), Vec2i(dimensions) });

    for (SizeType i = before_index + 1; i < free_spaces.Size(); i++)
    {
        auto& space_offset = free_spaces[i].first;
        auto& space_dimensions = free_spaces[i].second;

        if (space_offset.x < offset.x + dimensions.x)
        {
            int shrink = offset.x + dimensions.x - space_offset.x;

            space_offset.x += shrink;
            space_dimensions.x -= shrink;

            if (space_dimensions.x <= 0)
            {
                free_spaces.EraseAt(i);
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    MergeSkyline();
}

template <class AtlasElement>
void AtlasPacker<AtlasElement>::MergeSkyline()
{
    for (SizeType i = 0; i < free_spaces.Size() - 1;)
    {
        int y0 = free_spaces[i].first.y;
        int y1 = free_spaces[i + 1].first.y;

        if (y0 == y1)
        {
            free_spaces[i].second.x += free_spaces[i + 1].second.x;
            free_spaces.EraseAt(i + 1);
        }
        else
        {
            i++;
        }
    }
}

} // namespace hyperion

#endif
