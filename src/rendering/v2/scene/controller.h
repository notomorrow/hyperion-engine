#ifndef HYPERION_V2_CONTROLLER_H
#define HYPERION_V2_CONTROLLER_H

#include "../components/base.h"

#include <type_traits>

namespace hyperion::v2 {

class Node;

struct ControllerInstanceData {
    using ControllerFunction       = std::add_pointer_t<void(Node *this_node)>;
    using ControllerUpdateFunction = std::add_pointer_t<void(Node *this_node, float delta)>;

    ControllerFunction       on_added;
    ControllerFunction       on_removed;
    ControllerUpdateFunction on_update;
};

template <class T>
class Controller : public EngineComponentBase<STUB_CLASS(Controller<T>)> {
    static_assert(std::is_base_of_v<ControllerInstanceData, T>);

    friend class Node;

public:
    Controller(const char *name, T &&instance_data)
        : m_name(nullptr),
          m_instance_data(std::move(instance_data))
    {
        if (name != nullptr) {
            size_t len = std::strlen(name);

            m_name = new char[len + 1];
            std::strncpy(m_name, name, len);
        }
    }

    Controller(const Controller &other) = delete;
    Controller &operator=(const Controller &other) = delete;

    ~Controller()
    {
        if (m_name != nullptr) {
            delete[] m_name;
        }
    }

private:
    char *m_name;
    T m_instance_data;
};

} // namespace hyperion::v2

#endif