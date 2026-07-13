#include "flycam.h"
#include "job-manager.h"
#include "pipelines/deferred-pipeline.h"
#include "pipelines/frame.h"
#include "pipelines/geometry-pipeline.h"
#include "scene.h"
#include "svet/renderer/memory.h"
#include "svet/renderer/staging.h"
#include "svet/renderer/swapchain.h"
#include "time-keeping.h"

#include <SDL3/SDL.h>
#include <ring-buffer/spsc_rb.hpp>

#include <print>

// Helper for RenderDoc-only crash debugging
#define PAUSE_HERE                                                             \
  {                                                                            \
    SDL_Event ee;                                                              \
    SDL_PollEvent(&ee);                                                        \
    while (ee.type != SDL_EventType::SDL_EVENT_KEY_DOWN ||                     \
           ee.key.key != SDLK_SPACE) {                                         \
      SDL_PollEvent(&ee);                                                      \
    }                                                                          \
  }

int main(int argc, char *argv[]) {
  using namespace svet::renderer;

  auto modelsPath = std::getenv("GLTF_MODELS_PATH");
  if (not modelsPath) {
    std::println("Please set GLTF_MODELS_PATH environment variable to where "
                 "you have checked out the Khronos Sample repo "
                 "(https://github.com/KhronosGroup/glTF-Sample-Models.git)");
    std::println("Windows>> $env:GLTF_MODELS_PATH = \"path\"");
    std::println("Linux>> export GLTF_MODELS_PATH=\"path\"");
    return 1;
  }

  const uint32_t width = 1600, height = 1000;
  SDL_Init(SDL_INIT_VIDEO);
  auto window = SDL_CreateWindow("window", width, height, SDL_WINDOW_VULKAN);
  RendererSpecification rendererSpec{};
  rendererSpec.allowValidation = true;
  auto context = init(window, rendererSpec);

  //
  // MEMORY POOL
  //
  MemoryPool uniformBufferPool;
  {
    MemoryPoolSpecification uniformBufferPoolSpec{};
    uniformBufferPoolSpec.bufferUsage = BufferUsage::UNIFORM;
    uniformBufferPoolSpec.properties =
        MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT;
    uniformBufferPoolSpec.size = 512;
    uniformBufferPool = createMemoryPool(context, uniformBufferPoolSpec);
  }

  MemoryPool geometryBufferPool;
  {
    MemoryPoolSpecification geometryPoolSpec{};
    geometryPoolSpec.bufferUsage =
        BufferUsage::VERTEX | BufferUsage::INDEX | BufferUsage::TRANSFER_DST;
    geometryPoolSpec.properties = MemoryProperties::DEVICE_LOCAL;
    // 32MB
    geometryPoolSpec.size = 33554432;
    geometryBufferPool = createMemoryPool(context, geometryPoolSpec);
  }

  MemoryPool textureImagePool;
  {
    MemoryPoolSpecification texturePoolSpec{};
    texturePoolSpec.imageUsage = ImageUsage::SAMPLED;
    texturePoolSpec.imageTiling = ImageTiling::OPTIMAL;
    texturePoolSpec.properties = MemoryProperties::DEVICE_LOCAL;
    // 512MB
    texturePoolSpec.size = 536870912;
    textureImagePool = createMemoryPool(context, texturePoolSpec);
  }

  MemoryPool targetImagePool;
  {
    MemoryPoolSpecification targetPoolSpec{};
    // This usage seems to be enough for all images right now, but I need to
    // look into it.
    targetPoolSpec.imageUsage = ImageUsage::SAMPLED;
    targetPoolSpec.imageTiling = ImageTiling::OPTIMAL;
    targetPoolSpec.properties = MemoryProperties::DEVICE_LOCAL;
    // 128MB
    targetPoolSpec.size = 134217728;
    targetImagePool = createMemoryPool(context, targetPoolSpec);
  }

  //
  // STAGING BUFFER
  //
  StagingBuffer stagingBuffer =
      createStagingBuffer(context, maxStagedUploadSize);

  //
  // DESCRIPTOR POOL
  //
  // TODO: Could go bindless, but I want to stay basic where possible
  DescriptorPoolSpecification descriptorPoolSpec{};
  descriptorPoolSpec.maxSets += 512;
  descriptorPoolSpec.uniformBufferCount = 256;
  descriptorPoolSpec.combinedSamplerCount = 256;
  descriptorPoolSpec.sampledImageCount = 256;
  descriptorPoolSpec.storageImageCount = 256;
  DescriptorPool descriptorPool =
      createDescriptorPool(context, descriptorPoolSpec);

  //
  // SCENE
  //
  SceneSpecification sceneSpec{};
  sceneSpec.uniformBufferPool = uniformBufferPool;
  sceneSpec.textureImagePool = textureImagePool;
  sceneSpec.stagingBuffer = stagingBuffer;
  sceneSpec.descriptorPool = descriptorPool;
  Scene scene = createScene(context, sceneSpec);
  scene.camera.position = glm::vec3(-350, 0, 50);
  scene.camera.orientation = glm::quat(1, 0, 0, 0);
  scene.camera.fov = 60.f;
  scene.camera.aspect = width / float(height);
  scene.camera.nearZ = 10.f;
  scene.camera.farZ = 5000.f;
  scene.sun.direction = glm::normalize(glm::vec3(0.1, -0.3, 1));
  scene.sun.intensity = 30.f;
  scene.sun.color = glm::vec3(1);

  //
  // JOB MANAGER
  //
  JobManager jobManager;
  initJobManager(jobManager, context, stagingBuffer);

  //
  // MODELS
  //
  std::string sponzaPath =
      std::format("{}/2.0/Sponza/glTF/Sponza.gltf", modelsPath);
  {
    AssimpLoadJob sponzaLoad{};
    sponzaLoad.file = sponzaPath.c_str();
    sponzaLoad.rotateZUp = true;
    sponzaLoad.scale = 1.f;
    sponzaLoad.descriptorPool = descriptorPool;
    sponzaLoad.sampler = scene.sampler;
    sponzaLoad.bufferPool = geometryBufferPool;
    sponzaLoad.imagePool = textureImagePool;
    appendAssimpLoadJob(jobManager, std::move(sponzaLoad));
  }

  //
  // DRAW PROCESSOR
  //
  DrawProcessor drawProcessor = createDrawProcessor(context);

  //
  // PIPELINE
  //
  PixelFormat colorPixelFormat = PixelFormat::UNORM8_RGBA;
  PixelFormat positionPixelFormat = PixelFormat::FLOAT16_RGBA;
  PixelFormat depthPixelFormat = PixelFormat::FLOAT32_D;

  DeferredPipelineSpecification deferredPipelineSpec{};
  deferredPipelineSpec.width = width;
  deferredPipelineSpec.height = height;
  deferredPipelineSpec.uniformBufferPool = uniformBufferPool;
  deferredPipelineSpec.targetImagePool = targetImagePool;
  deferredPipelineSpec.shadowMapResolution = 2048;
  deferredPipelineSpec.shadowMapPixelFormat = PixelFormat::UNORM16_D;
  deferredPipelineSpec.colorPixelFormat = colorPixelFormat;
  deferredPipelineSpec.positionPixelFormat = positionPixelFormat;
  deferredPipelineSpec.depthPixelFormat = depthPixelFormat;
  deferredPipelineSpec.descriptorPool = descriptorPool;
  DeferredPipeline deferredPipeline =
      createDeferredPipeline(context, deferredPipelineSpec);

  GeometryPipelineSpecification geometryPipelineSpec{};
  geometryPipelineSpec.width = width;
  geometryPipelineSpec.height = height;
  geometryPipelineSpec.targetImagePool = targetImagePool;
  geometryPipelineSpec.colorPixelFormat = colorPixelFormat;
  geometryPipelineSpec.depthPixelFormat = depthPixelFormat;
  GeometryPipeline geometryPipeline =
      createGeometryPipeline(context, geometryPipelineSpec);

  //
  // SWAPCHAIN
  //
  Swapchain swapchain;
  {
    SwapchainSpecification swapchainSpec{};
    swapchainSpec.width = width;
    swapchainSpec.height = height;
    swapchainSpec.vSync = true;
    swapchainSpec.stabilizeFramerate = 60;
    swapchain = createSwapchain(context, swapchainSpec);
  }

  //
  // RENDER LOOP
  //
  FrameData frame;
  frame.context = context;
  frame.drawProcessor = drawProcessor;
  bool shouldEnd = false;
  int seconds = 0;
  int frames = 0;
  float timeDelta = 0.f;
  float sunTime = 1.4f;
  bool sunPassageEnabled = false;
  uint32_t selectedBone;
  enum class DrawnPipeline : uint32_t {
    GEOMETRY,
    DEFERRED,
    MAX,
  } drawnPipeline = DrawnPipeline::GEOMETRY;
  float renderingStartSeconds = getTimeSeconds();
  while (not shouldEnd) {
    frame.memory.reset();
    frame.timeSeconds = getTimeSeconds() - renderingStartSeconds;
    if (frame.timeSeconds > seconds + 1) {
      std::println("FPS: {}", frames);
      frames = 0;
      ++seconds;
    }

    //
    // EVENTS
    //
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EventType::SDL_EVENT_KEY_DOWN) {
        if (event.key.key == SDLK_ESCAPE) {
          shouldEnd = true;
        }
        if (event.key.key == SDLK_SPACE) {
          drawnPipeline = (DrawnPipeline)(((uint32_t)drawnPipeline + 1) %
                                          (uint32_t)DrawnPipeline::MAX);
        }
        if (event.key.key == SDLK_G) {
          sunPassageEnabled = not sunPassageEnabled;
        }
      }

      if (event.type == SDL_EventType::SDL_EVENT_QUIT) {
        shouldEnd = true;
      }
    }

    //
    // JOB PROCESSING
    //
    // Budgeting 1 millisecond to the job manager dispatch jobs to the job
    // thread and to process incoming results
    updateJobManager(jobManager, scene, 0.001f);

    //
    // UPDATE
    //
    flycamUpdate(scene.camera, timeDelta);
    auto vp = getVP(scene.camera);
    uploadBufferData(context, scene.cameraBuffer, &vp, 0, sizeof(glm::mat4));

    if (sunPassageEnabled)
      sunTime += 0.005f * timeDelta;
    scene.sun.direction.y = cos(sunTime);
    scene.sun.direction.z = sin(sunTime);
    scene.sun.direction.x = -0.2;
    scene.sun.direction = glm::normalize(scene.sun.direction);

    //
    // DRAW PROCESSING
    //
    Image drawTarget;
    ImageMetadata drawTargetMeta;
    switch (drawnPipeline) {
    case DrawnPipeline::GEOMETRY:
      geometryPipelineDrawFrame(frame, geometryPipeline, scene, drawTarget,
                                drawTargetMeta);
      break;
    case DrawnPipeline::DEFERRED:
      deferredPipelineDrawFrame(frame, deferredPipeline, scene, drawTarget,
                                drawTargetMeta);
      break;
    default:
      std::println("Unknown pipeline type requested.");
    }

    //
    // PRESENT
    //
    present(context, swapchain, drawTarget, drawTargetMeta, nullptr);
    ++frames;

    timeDelta = (getTimeSeconds() - renderingStartSeconds) - frame.timeSeconds;
  }

  stopJobManager(jobManager);
  // Updating one more time to be able to clean up finished jobs
  updateJobManager(jobManager, scene, 2.f);

  //
  // CLEANUP
  //
  destroySwapchain(context, swapchain);
  destroyGeometryPipeline(context, geometryPipeline);
  destroyDeferredPipeline(context, deferredPipeline);
  destroyDrawProcessor(context, drawProcessor);
  destroyScene(context, scene);
  destroyDescriptorPool(context, descriptorPool);
  destroyStagingBuffer(context, stagingBuffer);
  std::print("TARGETS - ");
  destroyMemoryPool(context, targetImagePool);
  std::print("TEXTURES - ");
  destroyMemoryPool(context, textureImagePool);
  std::print("GEOMETRY - ");
  destroyMemoryPool(context, geometryBufferPool);
  std::print("UNIFORMS - ");
  destroyMemoryPool(context, uniformBufferPool);
  shutdown(context);

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
