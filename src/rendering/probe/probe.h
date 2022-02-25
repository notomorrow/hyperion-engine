#ifndef PROBE_H
#define PROBE_H

#include "probe_camera.h"
#include "../renderable.h"
#include "../../math/matrix4.h"
#include "../../math/vector3.h"

#include <memory>
#include <array>

namespace hyperion {
class Probe : public Renderable {
public:
    enum ProbeType {
        PROBE_TYPE_NONE = 0,
        PROBE_TYPE_ENVMAP,
        PROBE_TYPE_VCT,
        PROBE_TYPE_SH,
        PROBE_TYPE_LIGHT_VOLUMES
    };

    Probe(const fbom::FBOMType &loadable_type, ProbeType probe_type, const Vector3 &origin, const BoundingBox &bounds);
    Probe(const Probe &other) = delete;
    Probe &operator=(const Probe &other) = delete;
    virtual ~Probe();

    inline ProbeType GetProbeType() const { return m_probe_type; }
    inline const std::shared_ptr<Texture> &GetTexture() const { return m_rendered_texture; }

    virtual void Update(double dt) = 0;
    virtual void Render(Renderer *renderer, Camera *cam) = 0;

    inline const BoundingBox &GetBounds() const { return m_bounds; }
    void SetBounds(const BoundingBox &bounds);
    inline const Vector3 &GetOrigin() const { return m_origin; }
    void SetOrigin(const Vector3 &origin);

    inline const std::array<ProbeCamera*, 6> &GetCameras() const { return m_cameras; }
    inline ProbeCamera *GetCamera(int index) { return m_cameras[index]; }
    inline const ProbeCamera *GetCamera(int index) const { return m_cameras[index]; }
    inline constexpr size_t NumCameras() const { return m_cameras.size(); }

protected:
    const ProbeType m_probe_type;
    BoundingBox m_bounds;
    Vector3 m_origin;
    std::array<ProbeCamera*, 6> m_cameras;
    /*const*/ std::array<std::pair<Vector3, Vector3>, 6> m_directions;
    std::shared_ptr<Texture> m_rendered_texture;
};
} // namespace hyperion

#endif
