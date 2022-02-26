# Hyperion Engine


## About

Hyperion Engine is an object-oriented 3D game engine written in C++ - it is easy to understand and build games with, while still giving you ample control over the game engine. It currently renders using OpenGL primarily but is in the process of being ported to Vulkan.

### Current features include:
* Physically based rendering
* Voxel Cone Tracing global illumination
* HDR tone mapping
* Parallax corrected dynamic cubemaps w/ spherical harmonics generation
* Procedural terrain generation
* Dynamic skydome with procedural clouds, skybox
* Deferred rendering with light optimzation - have 1000's of point lights
* Skeletal animation
* Post processing effects (SSAO, screen space reflections, depth of field, bloom, fxaa)
* Cascaded shadow maps, variance shadow maps
* Scene graph + octree
* Fast binary objects/models (FBOM) using object serialization/deserialization
* Bullet physics

Feel free to contribute anything - We'd love to have some more eyes on this project! Submit an issue if you run into anything, as well.

## Screenshots

![Apex engine screenshot](/screenshots/screenshot71.PNG)
![Apex engine screenshot](/screenshots/screenshot57.PNG)
![Apex engine screenshot](/screenshots/screenshot58.PNG)
![Apex engine screenshot](/screenshots/screenshot61.PNG)

## Building

This section will be updated soon. Hyperion uses a pretty simple CMake set up, using vcpkg for MSVC on Windows.
Make sure you install the required packages listed in CMakeLists.txt.
