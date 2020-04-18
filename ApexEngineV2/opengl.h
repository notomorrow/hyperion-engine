#ifndef OPENGL_H
#define OPENGL_H

#define USE_GLFW_ENGINE 1

#ifndef __APPLE__
#define USE_GLEW 1
#endif

#if USE_GLFW_ENGINE

#if USE_GLEW
#include <GL/glew.h>
#else
#include <OpenGL/gl3.h>
#endif

#include <GLFW/glfw3.h>

#endif

#endif