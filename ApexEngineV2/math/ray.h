#ifndef RAY_H
#define RAY_H

#include "vector3.h"

namespace apex {
struct Ray {
    Vector3 position;
    Vector3 direction;
};
}

#endif