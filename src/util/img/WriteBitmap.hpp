#ifndef WRITE_BITMAP_H
#define WRITE_BITMAP_H

#include <util/Defines.hpp>

// just a quick BMP writer for testing
// https://stackoverflow.com/a/47785639/8320593

namespace hyperion {

class WriteBitmap
{
public:
    HYP_API static void Write(
        const char *path,
        int width,
        int height,
        unsigned char *bytes
    );
};

} // namespace hyperion

#endif
