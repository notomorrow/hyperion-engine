#ifndef POPULATOR_H
#define POPULATOR_H

#include "../../entity.h"
#include "../../control.h"
#include "../../math/vector2.h"
#include "../../math/vector3.h"

#include <vector>
#include <memory>

namespace apex {
class TerrainChunk;
class Camera;
class Populator : public EntityControl {
public:
    Populator(
        Camera *camera,
        float tolerance = 0.15f,
        float max_distance = 700.0f,
        float spread = 5.0f,
        int num_patches = 1,
        int patch_spread = -1,
        bool use_batching = true
    );
    virtual ~Populator() = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(double dt) override;

    void CreatePatches(
        const Vector2 &origin,
        const Vector2 &center,
        int num_chunks,
        int num_entity_per_chunk,
        float parent_size
    );

protected:

    struct GridTile {
        float width;
        float length;
        float x;
        float z;
        Vector2 center;
        float max_distance;

        GridTile()
            : x(0.0),
              z(0.0),
              width(0.0),
              length(0.0),
              max_distance(0.0),
              center(Vector2::Zero())
        {
        }

        GridTile(float x, float z, float width, float length, float max_distance)
            : x(x),
              z(z),
              width(width),
              length(length),
              max_distance(max_distance),
              center(x + width / 2.0, z + width / 2.0)
        {
        }

        GridTile(const GridTile &other)
            : x(other.x),
              z(other.z),
              width(other.width),
              length(other.length),
              max_distance(other.max_distance),
              center(other.center)
        {
        }

        inline bool Collides(const Vector2 &point, const Vector2 &target, float size)
        {
            if (MathUtil::Round(point.x) >= target.x - size && MathUtil::Round(point.x <= target.x + size)) {
                if (MathUtil::Round(point.y) >= target.y - size && MathUtil::Round(point.y <= target.y + size)) {
                    return true;
                }
            }

            return false;
        }

        inline bool InRange(const Vector3 &point)
        {
            float dist = center.Distance(Vector2(point.x, point.z));

            return dist < max_distance;
        }
    };

    struct Patch {
        std::vector<std::shared_ptr<Entity>> m_entities;
        Vector2 m_terrain_patch_loc;
        Vector3 m_chunk_start;
        bool m_is_created;
        float m_chunk_size;
        GridTile m_tile;

        enum PageState {
            PATCH_WAITING,
            PATCH_UNLOADED,
            PATCH_UNLOADING,
            PATCH_LOADED
        } m_page_state;
    };

    Camera *m_camera;

    std::shared_ptr<Entity> m_entity;

    std::vector<Patch> m_patches;
    float m_tolerance;
    float m_max_distance;
    float m_spread;
    int m_num_patches;
    int m_patch_spread;
    bool m_use_batching;
};
} // namespace apex

#endif
