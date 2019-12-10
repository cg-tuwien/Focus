# Focus - A Game based on Ray Tracing

![screenshots](img/screenshot.png)

## About the Game
This game uses real-time raytracing for gameplay mechanics. A raytracing supporting graphic card, such as models from the NVIDIA RTX series is therefore necessary to play the game. Inside each level, there is a yellow sphere (the so called "Focusphere"), that brightens up the sky when you look at it. You win the level, if you stand on the goal platform (recognizable by its beige color) and look at the sphere, such that the sky reaches its maximum brightness. Sometimes it's not possible to look at the sphere directly from the goal platform, so you need to use rotatable mirrors in the level to look at the sphere.

Controls:
* WASD + Mouse Movement: Move around
* Space: Jump
* Left Click: Interact with Mirror
* Tab: Pause Game
* Esc: Exit Game
* F10: Skip Level

## Technical Overview
This game is based on the framework [cgbase](https://github.com/cg-tuwien/cg_base) using Vulkan as the underlying graphics API. One of the main concepts of cgbase is the _composition_, which capsules several _cg\_elements_, which define the behaviour of different components of the game. Each cg\_element has own methods for initializing, updating, rendering etc., which are called automatically by the framework. This game uses four types of cg\_element, which are:
* [fgamecontrol](source_code/fgamecontrol.h): Manages the game on a high level. Creates other cg\_element and is responsible for changing the levels, as well as pausing and stopping the game.
* [flevellogic](source_code/flevellogic.h): Describes the mechanics of a level. Animates the objects, moves the player, and checks for win/loose-conditions. Each level has its own subclass. Classes such as [fplayercontrol](source_code/fplayercontrol.h) and [fphysicscontroller](source_code/fphysicscontroller.h) help simplyfing the code of these classes.
* [fscene](source_code/fscene.h): Capsules all the scene objects of a level. Also creates and manages the GPU buffers and raytracing acceleration structures, which are updated, when objects in the scene change.
* [frenderer](source_code/frenderer.h): Responsible for starting the rendering process and initializing all the necessary data that is needed for that, such as descriptor sets and command buffers.

Note that in a classical rasterization based game, you could create an own cg\_element for each scene object with its own render-function. In a raytracing based game however, this is not really an option, as there is only one top level acceleration structure for the entire scene, which has to be passed to the ray generation shader once as a whole.

An important aspect of Vulkan is that we have several frames in flight, where the next frame might be started to be processed, while the last one is not entirely finished. For this reason, all scene data that might change during the game has to be stored several times on the GPU, once for each frame, such that updates of the data only affect the next frames, and no frames which are already being processed.

The core gameplay mechanic of this game is the brightening of the sky when the sphere is looked at (directly or through a mirror). To do this, we store a payload object of type RayTracingHit inside the shaders while recursing through the reflections. This object contains various informations, such as the color, information about transparent objects along the ray and some other info, among which is the information whether the ray has hit the focusphere. This variable is set in the Any-Hit-Shader (as the sphere is a transparent object) and is eventually passed on to the parent of the current ray tracing call. The ray generation shader, which creates one ray per pixel, is then able to check this property for each ray. It sums up the amount of pixels inside the central screen region using the function _atomicAdd_ on a shader storage buffer objects (note that there are no atomic counters in Vulkan). Inside the frenderer class, the value inside the SSBO is then read and reset. It is divided by the size of the screen to get a screen-size-independent value describing how much of the sphere is inside the central region of the screen. As we have several frames in flight, we can only read the value for the current frame a few frames later. However this is such a small delay, that it is basically not perceptible.

![class diagram](img/ClassDiagram.png)

## Installation

Follow these steps:    
1. Clone the repository
2. Update the git submodules (command: `git submodule update --init`)
3. Open [`visual_studio/focus_rt.sln`](./visual_studio/the_game) with Visual Studio 2019
4. Select `focus_rt` as startup project.
5. Make sure that a Vulkan configuration is selected
6. Build
7. Run

Detailed information about project setup and resource management with Visual Studio are given in [`cg_base/visual_studio/README.md`](https://github.com/cg-tuwien/cg_base/tree/master/visual_studio/README.md).
