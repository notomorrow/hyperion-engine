#ifndef COLLISION_DATA_H
#define COLLISION_DATA_H

#include "contact.h"

#include <array>

namespace apex {
struct CollisionData {
    std::array<Contact, MAX_CONTACTS> m_contacts;

    unsigned int m_contact_index;
    unsigned int m_contact_count;
    int m_contacts_left;

    double m_friction;
    double m_restitution;
    double m_tolerance;

    CollisionData();
    ~CollisionData();

    void Reset();
    void AddContacts(unsigned int count);
};
} // namespace apex

#endif