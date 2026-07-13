# WIP

This project is a work in progress. I made it public only to showcase as part of my portfolio. This is why I haven't bother with implementing frames in flight, because my machine is steadily drawing at 60 FPS still. Also, since I'm prototyping, I prefer a data-oriented approach, which I know to be faster and less constraining. I will switch to OOP at higher levels once the architecture is stable. I'm also using address sanitizer to track down memory leaks in the meantime.

Note, that `ext/pool-allocator` and `ext/ring-buffer` are both my implementations of the respective data structures. I purposefully aimed for a STL-like feel to correlate with the usage of similar data structures in STL.

I made this on Linux and haven't had the chance to test on Windows, but I haven't used any libraries that should prevent a successful (maybe even trivial) port.

# Features
- Deferred PBR rendering of opaque objects
- Weighted OIT (order-independent transparency) with PBR forward shading for transparent objects
- Shadow mapping with PCF (percentage closer filtering)
- Asset loading from secondary thread using the GPUs DMA-only engine via dedicated transfer queue
- VSync and frame stabilization
- .. and more

# TODO
- Transparent object rendering via sorting + alpha-to-coverage using MSAA
- Multisampling
- Mip-mapping
- Performance optimization

# Sponza

I'm using the `https://github.com/KhronosGroup/glTF-Sample-Assets.git` 2.0 Sponza model (`glTF-Sample-Assets/2.0/Sponza/glTF/Sponza.gltf`) to test. You can use *W*,*S*,*A*,*D*,*R*,*F* and *mouse drag-and-move* to control the flycam. *SPACE* switches between simple geometry rendering and the full deferred pipeline. *G* enables or disables the directional light rotating around the scene.
