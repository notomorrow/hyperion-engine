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

template <class T, LoaderFormat Format>
struct VoidLoader {};

/* Raw data representing a deserialized form of the resource
 * Not the final result - can be a simple struct overloaded to add members
 */
template <class T, LoaderFormat Format>
struct LoaderObject {
    //using Loader = VoidLoader<T, Format>;
};

} // namespace hyperion::v2

#endif