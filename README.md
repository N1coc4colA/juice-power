# juice-power
A small platformer game about juice.
Currently in its primices.

## Dependencies

### Resource Building Dependencies
You need the following tools to build resources used by the project:
```
 GLSLC
```

### Build Dependencies
```
 SDL3 (Because it's easy to show a window using it)
 magic_enum (Mapping enum values and their names)
 Vulkan (To use your GPU)
 fmt (C++ formatting lib)
 VMA (AMD's Vulkan Memory Allocator)
```

## Build Steps
Run the following commands from the root dir to build the project:
```
 $ mkdir build && cd build
 $ cmake ../
 $ make
```
