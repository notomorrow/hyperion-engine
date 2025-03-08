/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIGHTMAP_VOLUME_HPP
#define HYPERION_LIGHTMAP_VOLUME_HPP

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/utilities/UUID.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

namespace hyperion {

HYP_CLASS()
class HYP_API LightmapVolume : public HypObject<LightmapVolume>
{
    HYP_OBJECT_BODY(LightmapVolume);

public:
    LightmapVolume();
    
    LightmapVolume(const BoundingBox &aabb);

    LightmapVolume(const LightmapVolume &other)             = delete;
    LightmapVolume &operator=(const LightmapVolume &other)  = delete;
    ~LightmapVolume();

    HYP_METHOD(Property="UUID", Serialize=true)
    HYP_FORCE_INLINE const UUID &GetUUID() const
        { return m_uuid; }

    /*! \internal For serialization only */
    HYP_METHOD(Property="UUID", Serialize=true)
    HYP_FORCE_INLINE void SetUUID(const UUID &uuid)
        { m_uuid = uuid; }

    HYP_METHOD(Property="AABB", Serialize=true)
    HYP_FORCE_INLINE const BoundingBox &GetAABB() const
        { return m_aabb; }

    HYP_METHOD(Property="AABB", Serialize=true)
    HYP_FORCE_INLINE void SetAABB(const BoundingBox &aabb);

    void Init();

private:
    UUID        m_uuid;
    BoundingBox m_aabb;
};

} // namespace hyperion

#endif
