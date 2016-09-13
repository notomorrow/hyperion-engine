#ifndef CONTACT_RESOLVER_H
#define CONTACT_RESOLVER_H

#include "contact.h"

#include <vector>
#include <array>

#define VELOCITY_EPSILON 0.01
#define POSITION_EPSILON 0.003

namespace apex {
class ContactResolver {
public:
    unsigned int m_velocity_iterations_used;
    unsigned int m_position_iterations_used;

    ContactResolver(unsigned int num_iterations);

    void SetNumIterations(unsigned int num_iterations);
    void ResolveContacts(std::array<Contact, MAX_CONTACTS> &contacts, unsigned int num_contacts, double dt);

protected:
    unsigned int m_num_iterations;

    void PrepareContacts(std::array<Contact, MAX_CONTACTS> &contacts, unsigned int num_contacts, double dt);
    void AdjustVelocities(std::array<Contact, MAX_CONTACTS> &contacts, unsigned int num_contacts, double dt);
    void AdjustPositions(std::array<Contact, MAX_CONTACTS> &contacts, unsigned int num_contacts, double dt);
};
} // namespace apex

#endif