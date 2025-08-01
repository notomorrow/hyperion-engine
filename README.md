# Hyperion Engine

Hyperion Engine is a 3D game engine written in C++. Currently, it targets Windows, macOS and Linux and has support for C# scripting via .NET Core.

To get started, check out the [Compiling the Engine](doc/CompilingTheEngine.md) guide to set up your development environment and compile the engine.


---

Dynamic Diffuse Global Illumination (DDGI) | GPU Particles
:-----------------------------------------:|:-------------------------:
![DDG(](/screenshots/ddgi.gif)             | ![GPU particles](/screenshots/particles.gif)

### Some features include:
* Multi threading with task system
* Entity component system
* PBR and post processing
* Hardware ray tracing support for reflections and global illumination
* Skeletal animation
* C# scripting using .NET Core
* More reflection and global illumination techniques such as
     * Screen space reflections
     * Voxel cone tracing
     * Environment probes
* GPU occlusion culling
* GPU particles