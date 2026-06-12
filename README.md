# Ocean Match 3

Ocean Match 3 is a shader-based 3D Match-3 puzzle game developed with C++ and OpenGL for the COMP410 Computer Graphics final project.

The game combines classic Match-3 mechanics with computer graphics concepts such as shader programming, geometric transformations, texture mapping, animation systems, special tile effects, and an interactive underwater environment.

## Technologies

- C++
- OpenGL
- GLSL
- GLFW
- GLM
- Assimp
- stb_image
- CMake

## Build and Run

### Requirements

Install the required dependencies:

```bash
brew install cmake glfw glm assimp jpeg zlib
```

### Build

```bash
rm -rf build
cmake -S . -B build
cmake --build build
```

### Run

```bash
./build/OceanMatch3
```

## Controls

- Mouse Click: Select and swap tiles
- W / A / S / D: Move background camera
- R: Reset background camera

## Course Concepts Used

- Graphics Pipeline
- Shader Programming
- Geometric Transformations
- Texture Mapping
- Procedural Geometry
- Animation Systems
- Camera Systems
- Real-Time Rendering
- Lighting and Shading

## Team

Developed as a COMP410 Computer Graphics Final Project at Koç University.
