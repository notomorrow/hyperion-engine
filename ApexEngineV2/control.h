#ifndef CONTROL_H
#define CONTROL_H

namespace apex {

class Entity;

class EntityControl {
    friend class Entity;
public:
    EntityControl(const double tps = 30.0);
    virtual ~EntityControl();

    virtual void OnAdded() = 0;
    virtual void OnRemoved() = 0;
    virtual void OnUpdate(double dt) = 0;

protected:
    Entity *parent;

private:
    const double tps;
    double tick;
};

} // namespace apex

#endif