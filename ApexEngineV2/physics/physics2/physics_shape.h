#ifndef APEX_PHYSICS_PHYSICS_SHAPE_H
#define APEX_PHYSICS_PHYSICS_SHAPE_H

namespace apex {
// forward declarations
class BoxPhysicsShape;
class SpherePhysicsShape;

enum PhysicsShapeType {
    PhysicsShape_box,
    PhysicsShape_sphere,
};

// abstract physics shape
class PhysicsShape {
public:
    virtual ~PhysicsShape() = default;

    inline PhysicsShapeType GetType() const { return shape; }

    virtual unsigned int CollideWith(BoxPhysicsShape *shape);
    virtual unsigned int CollideWith(SpherePhysicsShape *shape);

private:
     PhysicsShapeType shape;
};
} // namespace apex

#endif