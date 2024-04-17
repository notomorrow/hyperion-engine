# Hyperion Engine

Hyperion Engine is a 3D game engine written in C++. Currently, it targets Windows, macOS and Linux and has support for C# scripting via .NET Core. 

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


## Building

Hyperion uses CMake to configure and generate the build files for both C++ and C#. Depending on your target platform, make sure you have the following dependencies installed:

### Windows
* Visual Studio 2019 or newer
* vcpkg (and ensure VCPKG_INSTALLED_DIR environment variable is set)
* Vulkan SDK
* SDL 2
* OpenAL 
* Bullet (optional)
* FreeType (optional)
* GStreamer (optional, only required for WebRTC streaming)

### macOS
* Xcode build tools (Clang compiler)
* Homebrew (to install necessary dependencies)
* MoltenVK (Vulkan SDK wrapper for Metal)
* SDL 2
* OpenAL
* Bullet (optional)
* FreeType (optional)
* GStreamer (optional, only required for WebRTC streaming)

### Linux
* GCC or Clang
* Vulkan SDK
* SDL 2
* OpenAL
* Bullet (optional)
* FreeType (optional)
* GStreamer (optional, only required for WebRTC streaming)

## Submodules

You should also ensure that submodules for the repo are initialized and updated. The main ones you'll need are:
* .NET Core Runtime (https://github.com/dotnet/runtime)
* glslang (https://github.com/KhronosGroup/glslang)

Some optional submodules include:
* xatlas (If you plan to bake lightmaps) (https://github.com/jpcy/xatlas)
* libdatachannel (For WebRTC support) (https://github.com/paullouisageneau/libdatachannel)

```bash
git submodule update --init --recursive
```
