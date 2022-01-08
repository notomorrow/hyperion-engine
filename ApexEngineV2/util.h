#ifndef UTIL_H
#define UTIL_H

#include "./opengl.h"

#include <iostream>

namespace apex {
    inline void CatchGLErrors(const char *message, bool should_throw = true, bool recursive = true)
    {
        unsigned int errors[255] = { GL_NO_ERROR },
            error = GL_NO_ERROR,
            counter = 0;

        while ((error = glGetError()) != GL_NO_ERROR) {
            errors[counter++] = error;

            if (!recursive) {
                break;
            }

            if (counter == 255) { // max len
                break;
            }
        }

        for (unsigned int i = 0; i < counter; i++) {
            std::cout << message << std::endl;
            std::cout << "\tGL Error: " << (errors[i]) << std::endl;
        }

        if (should_throw && errors[0] != GL_NO_ERROR) {
            throw std::runtime_error(message);
        }
    }
} // namespace apex

#endif