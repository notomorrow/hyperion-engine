#include "contact_resolver.h"

namespace apex {
ContactResolver::ContactResolver(unsigned int num_iterations)
    : m_num_iterations(num_iterations)
{
}

void ContactResolver::SetNumIterations(unsigned int num_iterations)
{
    m_num_iterations = num_iterations;
}

void ContactResolver::ResolveContacts(std::array<Contact, MAX_CONTACTS> &contacts, unsigned int num_contacts, double dt)
{
    if (contacts.empty() || m_num_iterations <= 0) {
        return;
    }

    PrepareContacts(contacts, num_contacts, dt);
    AdjustPositions(contacts, num_contacts, dt);
    AdjustVelocities(contacts, num_contacts, dt);
}

void ContactResolver::PrepareContacts(std::array<Contact, MAX_CONTACTS> &contacts, unsigned int num_contacts, double dt)
{
    for (unsigned int i = 0; i < num_contacts; i++) {
        contacts[i].CalculateInternals(dt);
    }
}

void ContactResolver::AdjustVelocities(std::array<Contact, MAX_CONTACTS> &contacts, unsigned int num_contacts, double dt)
{
    Vector3 delta_velocity;
    std::array<Vector3, 2> velocity_change;
    std::array<Vector3, 2> rotation_change;

    m_velocity_iterations_used = 0;
    while (m_velocity_iterations_used < m_num_iterations) {
        double max = VELOCITY_EPSILON;
        unsigned int index = num_contacts;
        for (unsigned int i = 0; i < num_contacts; i++) {
            auto &contact = contacts[i];
            if (contact.m_desired_delta_velocity > max) {
                max = contact.m_desired_delta_velocity;
                index = i;
            }
        }

        if (index == num_contacts) {
            break;
        }

        contacts[index].MatchAwakeState();
        contacts[index].ApplyVelocityChange(velocity_change, rotation_change);

        for (unsigned int i = 0; i < num_contacts; i++) {
            for (unsigned int b = 0; b < 2; b++) {
                if (contacts[i].m_bodies[b] != nullptr) {
                    for (unsigned int d = 0; d < 2; d++) {
                        if (contacts[i].m_bodies[b] == contacts[index].m_bodies[d]) {
                            delta_velocity = velocity_change[d] +
                                (rotation_change[d] * contacts[i].m_relative_contact_position[b]);

                            Matrix3 contact_transpose = contacts[i].m_contact_to_world;
                            contact_transpose.Transpose();

                            contacts[i].m_contact_velocity += delta_velocity * contact_transpose * (b ? -1 : 1);
                            contacts[i].CalculateDesiredDeltaVelocity(dt);
                        }
                    }
                }
            }
        }

        m_velocity_iterations_used++;
    }
}

void ContactResolver::AdjustPositions(std::array<Contact, MAX_CONTACTS> &contacts, unsigned int num_contacts, double dt)
{
    std::array<Vector3, 2> linear_change;
    std::array<Vector3, 2> angular_change;
    double max;
    Vector3 delta_position;

    m_position_iterations_used = 0;
    while (m_position_iterations_used < m_num_iterations) {
        max = POSITION_EPSILON;
        unsigned int index = num_contacts;
        for (unsigned int i = 0; i < num_contacts; i++) {
            if (contacts[i].m_contact_penetration > max) {
                max = contacts[i].m_contact_penetration;
                index = i;
            }
        }

        if (index == num_contacts) {
            break;
        }

        contacts[index].MatchAwakeState();
        contacts[index].ApplyPositionChange(linear_change, angular_change, max);

        for (unsigned int i = 0; i < num_contacts; i++) {
            for (unsigned int b = 0; b < 2; b++) {
                if (contacts[i].m_bodies[b] != nullptr) {
                    for (unsigned int d = 0; d < 2; d++) {
                        if (contacts[i].m_bodies[b] == contacts[index].m_bodies[d]) {
                            delta_position = linear_change[d] +
                                (angular_change[d] * contacts[i].m_relative_contact_position[b]);

                            contacts[i].m_contact_penetration += delta_position.Dot(contacts[i].m_contact_normal) *
                                (b ? 1 : -1);
                        }
                    }
                }
            }
        }

        m_position_iterations_used++;
    }
}
} // namespace apex