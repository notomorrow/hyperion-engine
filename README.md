# Hyperion Engine


## About

Hyperion Engine is a 3D game engine written in C++, rendering on Vulkan.

The goal of Hyperion is to be easy to understand and build games with, while still giving you ample control over the game engine.

### Current features include:
* Physically based rendering
* Procedural, paged terrain generation
* Skeletal animation
* Serialization / deserialization system
* Highly multi-threaded with async task system
* Post processing pipeline - SSAO, Screen-space reflections, FXAA, Tonemapping
* Parallax corrected dynamic cubemap rendering
* Voxel Cone Tracing (global illumination and reflections)
* Contact hardened softened shadows
* Scene graph + octree
* Scripting engine (Hypscript)
* Ray tracing - DDGI implementation in development

Feel free to contribute anything - We'd love to have some more eyes on this project! Submit an issue if you run into anything, as well.

## Docs
In the works

## Screenshots

![screenshot](/screenshots/sponza-vk.png)
*The new Vulkan renderer*
![screenshot](/screenshots/ssr.png)
*Screenspace reflections*
![screenshot](/screenshots/parallax.png)
*Parallax occlusion mapping*
![screenshot](/screenshots/screenshot1-ogl.PNG)
*Old OpenGL renderer*

## Building

This section will be updated soon. Hyperion uses a pretty simple CMake set up, using vcpkg for MSVC on Windows, and a build script (`build.sh`) for macOS and Linux.

Make sure you install the required packages listed in CMakeLists.txt.

### Known issues
* Currently some resources are not being cleaned up properly on close
* Sometimes, may run into a race condition, some things need to be more thread-safe
* Scripting has issues with generic types
* Binding C++ variables to scripts still not fleshed out
