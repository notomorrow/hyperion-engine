#ifndef CONTROL_H
#define CONTROL_H

namespace apex {
class Entity;

class EntityControl {
public:
    virtual ~EntityControl()
    {
    }

    virtual void OnAdded() = 0;
    virtual void OnRemoved() = 0;
    virtual void OnUpdate(double dt) = 0;

protected:
    Entity *parent;
    friend class Entity;
};
}

#endif