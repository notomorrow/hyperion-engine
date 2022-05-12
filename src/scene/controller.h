#ifndef HYPERION_V2_CONTROLLER_H
#define HYPERION_V2_CONTROLLER_H

#include <rendering/base.h>
#include "../game_counter.h"

#include <type_traits>

namespace hyperion::v2 {

class Node;
class Engine;

class Controller {
    friend class Node;

public:
    Controller(const char *name);
    Controller(const Controller &other) = delete;
    Controller &operator=(const Controller &other) = delete;
    virtual ~Controller();

    const char *GetName() const { return m_name; }
    Node *GetParent() const     { return m_parent; }

protected:
    virtual void OnAdded() = 0;
    virtual void OnRemoved() = 0;
    virtual void OnUpdate(GameCounter::TickUnit delta) = 0;

    virtual void OnDescendentAdded(Node *descendent) {}
    virtual void OnDescendentRemoved(Node *descendent) {}

private:
    char *m_name;
    Node *m_parent;
};

} // namespace hyperion::v2

#endif