#ifndef HYPERION_V2_LOADER_OBJECT_H
#define HYPERION_V2_LOADER_OBJECT_H

#include <math/vector2.h>
#include <math/vector3.h>
#include <rendering/v2/components/node.h>

#include <string>
#include <vector>

namespace hyperion::v2 {

enum class LoaderFormat {
    NONE,

    MODEL_OBJ,
    MODEL_OGRE_XML,
    MODEL_FBOM,

    IMAGE_2D
};

/* Raw data representing a deserialized form of the resource
 * Not the final result - can be a simple struct overloaded to add members
 */
template <class T>
struct LoaderObject {};

template <>
struct LoaderObject<void> {
    LoaderObject() = delete;
};

} // namespace hyperion::v2

#endif