#include "complex_collision_detector.h"
#include "simple_collision_detector.h"

#include <cassert>

namespace apex {
static void FillPointFaceBoxBox(const CollisionBox &a, const CollisionBox &b,
    const Vector3 &to_center, CollisionData &data,
    unsigned int best, double penetration)
{
    Contact &contact = data.m_contacts[data.m_contact_index];

    Vector3 normal = a.GetAxis(best);
    if (a.GetAxis(best).Dot(to_center) > 0) {
        normal *= -1.0f;
    }

    Vector3 vertex = b.GetDimensions() * 0.5f;
    for (int i = 0; i < 3; i++) {
        if (b.GetAxis(i).Dot(normal) < 0) {
            vertex[i] *= -1.0f;
        }
    }

    contact.SetContactNormal(normal);
    contact.SetContactPenetration(penetration);
    contact.SetContactPoint(vertex * b.GetTransform());
    contact.SetBodyData(a.GetBody(), b.GetBody(),
        data.m_friction, data.m_restitution);
}

static Vector3 ContactPoint(const Vector3 &a_point, const Vector3 &a_dir, double a_size,
    const Vector3 &b_point, const Vector3 &b_dir, double b_size,
    bool outside_edge)
{
    double a_len_sqr = double(a_dir.LengthSquared());
    double b_len_sqr = double(b_dir.LengthSquared());
    double b_dot_a = b_dir.Dot(a_dir);

    Vector3 dist = a_point - b_point;
    double a_dist = a_dir.Dot(dist);
    double b_dist = b_dir.Dot(dist);

    double denom = (a_len_sqr * b_len_sqr) - (b_dot_a * b_dot_a);

    if (fabs(denom) < MathUtil::EPSILON) {
        return outside_edge ? a_point : b_point;
    }

    double mua = (b_dot_a * b_dist - b_len_sqr * a_dist) / denom;
    double mub = (a_len_sqr * b_dist - b_dot_a * a_dist) / denom;

    if (mua > a_size || mua < -a_size || mub > b_size || mub < -b_size) {
        return outside_edge ? a_point : b_point;
    } else {
        return (a_point + (a_dir * mua)) * 0.5 +
            (b_point + (b_dir * mub)) * 0.5;
    }
}

static double PenetrationOnAxis(const CollisionBox &a, const CollisionBox &b,
    const Vector3 &axis, const Vector3 &to_center)
{
    double a_proj = a.TransformToAxis(axis);
    double b_proj = b.TransformToAxis(axis);

    double dist = fabs(to_center.Dot(axis));

    return (a_proj + b_proj) - dist;
}

static bool TryAxis(const CollisionBox &a, const CollisionBox &b,
    Vector3 axis, const Vector3 &to_center, unsigned int index,
    double &out_smallest_penetration, unsigned int &out_smallest_case)
{
    if (axis.LengthSquared() < MathUtil::EPSILON) {
        return true;
    }
    axis.Normalize();

    double penetration = PenetrationOnAxis(a, b, axis, to_center);
    if (penetration < 0) {
        return false;
    }

    if (penetration < out_smallest_penetration) {
        out_smallest_penetration = penetration;
        out_smallest_case = index;
    }

    return true;
}

unsigned int ComplexCollisionDetector::SphereAndHalfSpace(const CollisionSphere &sphere, const CollisionPlane &plane,
    CollisionData &data)
{
    if (data.m_contacts_left <= 0) {
        return 0;
    }

    Vector3 position = sphere.GetAxis(3);

    double dist = plane.m_direction.Dot(position) - sphere.GetRadius() - plane.m_offset;
    if (dist >= 0) {
        return 0;
    }

    Vector3 contact_point = position - plane.m_direction *
        (dist - sphere.GetRadius());

    Contact &contact = data.m_contacts[data.m_contact_index];
    contact.SetContactNormal(plane.m_direction);
    contact.SetContactPenetration(-dist);
    contact.SetContactPoint(contact_point);
    contact.SetBodyData(sphere.GetBody(), nullptr,
        data.m_friction, data.m_restitution);

    data.AddContacts(1);
    return 1;
}

unsigned int ComplexCollisionDetector::SphereAndTruePlane(const CollisionSphere &sphere, const CollisionPlane &plane,
    CollisionData &data)
{
    if (data.m_contacts_left <= 0) {
        return 0;
    }

    Vector3 position = sphere.GetAxis(3);

    double dist = plane.m_direction.Dot(position) - plane.m_offset;
    if ((dist * dist) > (sphere.GetRadius() * sphere.GetRadius())) {
        return 0;
    }

    Vector3 contact_normal = plane.m_direction;
    double penetration = -dist;
    if (dist < 0) {
        contact_normal *= -1;
        penetration *= -1;
    }
    penetration += sphere.GetRadius();

    Vector3 contact_point = position - (plane.m_direction * dist);

    Contact &contact = data.m_contacts[data.m_contact_index];
    contact.SetContactNormal(contact_normal);
    contact.SetContactPenetration(penetration);
    contact.SetContactPoint(contact_point);
    contact.SetBodyData(sphere.GetBody(), nullptr,
        data.m_friction, data.m_restitution);

    data.AddContacts(1);
    return 1;
}

unsigned int ComplexCollisionDetector::SphereAndSphere(const CollisionSphere &a, const CollisionSphere &b,
    CollisionData &data)
{
    if (data.m_contacts_left <= 0) {
        return 0;
    }

    Vector3 a_position = a.GetAxis(3);
    Vector3 b_position = b.GetAxis(3);

    Vector3 mid = a_position - b_position;
    double mag = double(mid.Length());

    if (mag <= 0.0 || mag >= (a.GetRadius() + b.GetRadius())) {
        return 0;
    }

    Vector3 contact_normal = mid * (1.0 / mag);
    Vector3 contact_point = a_position + (mid / 2.0);
    double penetration = (a.GetRadius() + b.GetRadius()) - mag;

    Contact &contact = data.m_contacts[data.m_contact_index];
    contact.SetContactNormal(contact_normal);
    contact.SetContactPenetration(penetration);
    contact.SetContactPoint(contact_point);
    contact.SetBodyData(a.GetBody(), b.GetBody(),
        data.m_friction, data.m_restitution);

    data.AddContacts(1);
    return 1;
}

unsigned int ComplexCollisionDetector::BoxAndHalfSpace(const CollisionBox &box, const CollisionPlane &plane,
    CollisionData &data)
{
    if (data.m_contacts_left <= 0) {
        return 0;
    }

    // try simple intersection
    if (!SimpleCollisionDetector::BoxAndHalfSpace(box, plane)) {
        return 0;
    }

    static const double pts[8][3] = {
        { 1, 1, 1 }, { -1, 1, 1 }, { 1, -1, 1 }, { -1, -1, 1 },
        { 1, 1, -1 }, { -1, 1, -1 }, { 1, -1, -1 }, { -1, -1, -1 }
    };

    unsigned int index = data.m_contact_index;
    unsigned int contacts_used = 0;
    for (int i = 0; i < 8; i++) {
        Contact &contact = data.m_contacts[index];

        // calculate vertex position
        auto vertex = Vector3(pts[i][0], pts[i][1], pts[i][2]);
        vertex *= box.GetDimensions() * 0.5f;
        vertex *= box.GetTransform();

        double dist = vertex.Dot(plane.m_direction);
        if (dist <= plane.m_offset) {
            contact.SetContactNormal(plane.m_direction);
            contact.SetContactPenetration(plane.m_offset - dist);
            contact.SetContactPoint(plane.m_direction *
                (dist - plane.m_offset) + vertex);
            contact.SetBodyData(box.GetBody(), nullptr,
                data.m_friction, data.m_restitution);

            index++;
            contacts_used++;
            if (contacts_used == (unsigned int)data.m_contacts_left) {
                return contacts_used;
            }
        }
    }
    
    data.AddContacts(contacts_used);
    return contacts_used;
}

unsigned int ComplexCollisionDetector::BoxAndBox(const CollisionBox &a, const CollisionBox &b,
    CollisionData &data)
{
    Vector3 to_center = b.GetAxis(3) - a.GetAxis(3);

    double penetration = DBL_MAX;
    unsigned int best = std::numeric_limits<unsigned int>::max();

    for (int i = 0; i < 3; i++) {
        if (!TryAxis(a, b, a.GetAxis(i), to_center,
            i, penetration, best)) {
            return 0;
        }
    }

    for (int i = 0; i < 3; i++) {
        if (!TryAxis(a, b, b.GetAxis(i), to_center,
            i + 3, penetration, best)) {
            return 0;
        }
    }

    unsigned int best_single_axis = best;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            Vector3 axis = a.GetAxis(i);
            axis.Cross(b.GetAxis(j));

            unsigned int index = (i * 3 + j) + 6;

            if (!TryAxis(a, b, axis, to_center,
                index, penetration, best)) {
                return 0;
            }
        }
    }

    // check to make sure there was a result
    assert(best != std::numeric_limits<decltype(best)>::max());

    if (best < 3) {
        FillPointFaceBoxBox(a, b, to_center, data, best, penetration);
        data.AddContacts(1);
        return 1;
    } else if (best < 6) {
        FillPointFaceBoxBox(b, a, to_center * -1.0f, data, best - 3, penetration);
        data.AddContacts(1);
        return 1;
    } else {
        best -= 6;
        unsigned int a_axis_index = best / 3;
        unsigned int b_axis_index = best % 3;

        Vector3 a_axis = a.GetAxis(a_axis_index);
        Vector3 b_axis = b.GetAxis(b_axis_index);

        Vector3 axis = a_axis;
        axis.Cross(b_axis);
        axis.Normalize();

        if (axis.Dot(to_center) > 0) {
            axis *= -1.0f;
        }

        Vector3 a_point_on_edge = a.GetDimensions() * 0.5f;
        Vector3 b_point_on_edge = b.GetDimensions() * 0.5f;

        for (unsigned int i = 0; i < 3; i++) {
            if (i == a_axis_index) {
                a_point_on_edge[i] = 0;
            } else if (a.GetAxis(i).Dot(axis) > 0) {
                a_point_on_edge[i] *= -1;
            }

            if (i == b_axis_index) {
                b_point_on_edge[i] = 0;
            } else if (b.GetAxis(i).Dot(axis) < 0) {
                b_point_on_edge[i] *= -1;
            }
        }

        a_point_on_edge *= a.GetTransform();
        b_point_on_edge *= b.GetTransform();

        Vector3 vertex = ContactPoint(a_point_on_edge, a_axis, a.GetDimensions()[a_axis_index] * 0.5f,
            b_point_on_edge, b_axis, b.GetDimensions()[b_axis_index] * 0.5f,
            best_single_axis > 2);

        Contact &contact = data.m_contacts[data.m_contact_index];
        contact.SetContactNormal(axis);
        contact.SetContactPenetration(penetration);
        contact.SetContactPoint(vertex);
        contact.SetBodyData(a.GetBody(), b.GetBody(),
            data.m_friction, data.m_restitution);

        data.AddContacts(1);
        return 1;
    }
    return 0;
}

unsigned int ComplexCollisionDetector::BoxAndPoint(const CollisionBox &box, const Vector3 &point,
    CollisionData &data)
{
    Matrix4 inverse = box.GetTransform();
    inverse.Invert();

    Vector3 point_transformed = point * inverse;

    double min_depth = (box.GetDimensions().GetX() * 0.5f) - abs(point_transformed.GetX());
    if (min_depth < 0) {
        return 0;
    }

    Vector3 normal = box.GetAxis(0);
    if (point_transformed.GetX() < 0) {
        normal *= -1;
    }

    double depth = (box.GetDimensions().GetY() * 0.5f) - abs(point_transformed.GetY());
    if (depth < 0) {
        return 0;
    } else if (depth < min_depth) {
        min_depth = depth;
        normal = box.GetAxis(1);
        if (point_transformed.GetY() < 0) {
            normal *= -1;
        }
    }

    depth = (box.GetDimensions().GetZ() * 0.5f) - fabs(point_transformed.GetZ());
    if (depth < 0) {
        return 0;
    } else if (depth < min_depth) {
        min_depth = depth;
        normal = box.GetAxis(2);
        if (point_transformed.GetZ() < 0) {
            normal *= -1;
        }
    }

    Contact &contact = data.m_contacts[data.m_contact_index];
    contact.SetContactNormal(normal);
    contact.SetContactPenetration(min_depth);
    contact.SetContactPoint(point);
    contact.SetBodyData(box.GetBody(), nullptr,
        data.m_friction, data.m_restitution);

    data.AddContacts(1);
    return 1;
}

unsigned int ComplexCollisionDetector::BoxAndSphere(const CollisionBox &box, const CollisionSphere &sphere,
    CollisionData &data)
{
    Matrix4 transform = box.GetTransform();
    float det = transform.Determinant();
    Matrix4 inverse = transform;
    inverse.Invert();
    //inverse *= det;

    Vector3 center = sphere.GetAxis(3);
    Vector3 center_transformed = center * inverse;

    if (fabs(center_transformed.GetX()) - sphere.GetRadius() > (box.GetDimensions().GetX() * 0.5f) ||
        fabs(center_transformed.GetY()) - sphere.GetRadius() > (box.GetDimensions().GetY() * 0.5f) ||
        fabs(center_transformed.GetZ()) - sphere.GetRadius() > (box.GetDimensions().GetZ()* 0.5f)) {
        return 0;
    }

    Vector3 closest = Vector3::Zero();

    double distance = center_transformed.GetX();
    if (distance > box.GetDimensions().GetX() * 0.5f) {
        distance = box.GetDimensions().GetX() * 0.5f;
    }
    if (distance < -(box.GetDimensions().GetX() * 0.5f)) {
        distance = -(box.GetDimensions().GetX() * 0.5f);
    }
    closest.SetX(distance);

    distance = center_transformed.GetY();
    if (distance > box.GetDimensions().GetY() * 0.5f) {
        distance = box.GetDimensions().GetY() * 0.5f;
    } 
    if (distance < -(box.GetDimensions().GetY() * 0.5f)) {
        distance = -(box.GetDimensions().GetY() * 0.5f);
    }
    closest.SetY(distance);

    distance = center_transformed.GetZ();
    if (distance > box.GetDimensions().GetZ() * 0.5f) {
        distance = box.GetDimensions().GetZ() * 0.5f;
    } 
    if (distance < -(box.GetDimensions().GetZ()* 0.5f)) {
        distance = -(box.GetDimensions().GetZ()* 0.5f);
    }
    closest.SetZ(distance);

    distance = (closest - center_transformed).LengthSquared();
    if (distance > sphere.GetRadius() * sphere.GetRadius()) {
        return 0;
    }

    Vector3 closest_transformed = closest * box.GetTransform();

    Vector3 contact_normal = closest_transformed - center;
    contact_normal.Normalize();

    Contact &contact = data.m_contacts[data.m_contact_index];
    contact.SetContactNormal(contact_normal);
    contact.SetContactPenetration(sphere.GetRadius() - sqrt(distance));
    contact.SetContactPoint(closest_transformed);
    contact.SetBodyData(box.GetBody(), sphere.GetBody(),
        data.m_friction, data.m_restitution);

    data.AddContacts(1);
    return 1;
}
} // namespace apex