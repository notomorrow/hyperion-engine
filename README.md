# Hyperion Engine


## About

Hyperion Engine is a 3D game engine written in C++ - it is easy to understand and use, while still giving you ample control over the game engine. It currently renders using OpenGL primarily but is in the process of being ported to Vulkan.

### Current features include:
* Physically based rendering
* Voxel Cone Tracing global illumination
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

![Apex engine screenshot](/screenshots/screenshot33.PNG)
![Apex engine screenshot](/screenshots/screenshot34PNG.PNG)
![Apex engine screenshot](/screenshots/screenshot8.png)

## Building

Hyperion uses CMake to build. We have added a couple shell scripts to make the building process a bit more convenient, although there really aren't too many steps.

If it is your first time building the engine, you can run the `setup.sh` script from the root directory of the project. This will create the `build` folder, as well as copy all needed resources over from the `res` folder.
> NOTE: Currently, assets are loaded from `build/res`, so if you make a change to any shaders, textures or models here, you'll have to copy them over to the root `res` folder before pushing a change, or else that change will not be reflected in your commit.

After that process has completed, `cd` into the `build` folder. From here, you're able to run `make` directly and then run the engine test with `./hyperion`. We also have a `build.sh` script,which when ran from the root project folder, will do this whole process for you.

Really, all you need to know is:
* Run `setup.sh` to regenerate the makefile and copy assets from `res` into `build/res`
* Run `build.sh` to build the engine
* Run `./hyperion` to run the engine test
