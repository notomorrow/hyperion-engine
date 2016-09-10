#ifndef CONTACT_GENERATOR_H
#define CONTACT_GENERATOR_H

#include "contact.h"

namespace apex {
class ContactGenerator {
public:
    virtual ~ContactGenerator() = default;

    virtual unsigned int AddContact(Contact &contact, unsigned int limit) const = 0;
};
} // namespace apex

#endif