#include "collision_data.h"

namespace apex {
CollisionData::CollisionData()
    : m_contact_index(0), m_contact_count(0), m_contacts_left(MAX_CONTACTS)
{
}

CollisionData::~CollisionData()
{
}

void CollisionData::Reset()
{
    m_contact_count = 0;
    m_contact_index = 0;
    m_contacts_left = MAX_CONTACTS;
}

void CollisionData::AddContacts(unsigned int count)
{
    m_contact_count += count;
    m_contact_index += count;
    m_contacts_left -= count;
}
} // namespace apex