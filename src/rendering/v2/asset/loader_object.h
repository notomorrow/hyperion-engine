#ifndef HYPERION_V2_LOADER_OBJECT_H
#define HYPERION_V2_LOADER_OBJECT_H

#include <math/vector2.h>
#include <math/vector3.h>

#include <string>
#include <vector>

namespace hyperion::v2 {

enum class LoaderFormat {
    NONE,

    MODEL_OBJ,
    MODEL_OGRE_XML,
    MODEL_FBOM,

    TEXTURE_2D
};

/* Raw data representing a deserialized form of the resource
 * Not the final result - can be a simple struct overloaded to add members
 */
template <class T, LoaderFormat Format>
struct LoaderObject {};

} // namespace hyperion::v2

#endif