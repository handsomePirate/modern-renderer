#include "flycam.h"
#include "job-manager.h"
#include "pipelines/deferred-pipeline.h"
#include "pipelines/geometry-pipeline.h"
#include "renderer.h"
#include "scene.h"
#include "time-keeping.h"

#include <SDL3/SDL.h>
#include <ring-buffer/spsc_rb.hpp>

#include <print>
#include <vector>

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
  if (argc != 2) {
    std::println(
        "The program requires exactly one argument (path to asset), {} "
        "given.\nTo get the model that this program was tested on, clone "
        "'https://github.com/KhronosGroup/glTF-Sample-Assets.git' and use "
        "'glTF-Sample-Assets/2.0/Sponza/glTF/Sponza.gltf'",
        argc - 1);
    return 1;
  }

  // Preallocate memory for draw commands to avoid allocating in the render loop
  const uint32_t maxDrawOperations = 16384;
  std::vector<DrawOperation> drawOperations(maxDrawOperations);
  const uint32_t maxRenderPasses = 8;
  std::vector<RenderPass> renderPasses(maxRenderPasses);
  const uint32_t maxPipelines = 32;
  std::vector<Pipeline> pipelines(maxPipelines);
  const uint32_t maxDescriptorSetBindings = 1024;
  std::vector<DescriptorSetBinding> descriptorSetBindings(
      maxDescriptorSetBindings);
  const uint32_t maxPushConstants = 1024;
  std::vector<PushConstantUpload> pushConstants(maxPushConstants);
  const uint32_t maxImageBarriers = 512;
  std::vector<ImageBarrier> imageBarriers(maxImageBarriers);
  const uint32_t maxBufferBarriers = 512;
  std::vector<BufferBarrier> bufferBarriers(maxBufferBarriers);
  const uint32_t maxVertexBindings = 4096;
  std::vector<VertexBinding> vertexBindings(maxVertexBindings);
  const uint32_t maxIndexBindings = 4096;
  std::vector<IndexBinding> indexBindings(maxIndexBindings);
  const uint32_t maxDrawCalls = 4096;
  std::vector<DrawCall> drawCalls(maxDrawCalls);
  const uint32_t maxDispatchCalls = 1024;
  std::vector<DispatchCall> dispatchCalls(maxDispatchCalls);
  const uint32_t maxBlitCalls = 1024;
  std::vector<BlitCall> blitCalls(maxBlitCalls);
  DrawCommand drawCommand{};
  drawCommand.operations = drawOperations.data();
  drawCommand.renderPasses = renderPasses.data();
  drawCommand.pipelines = pipelines.data();
  drawCommand.descriptorSetBindings = descriptorSetBindings.data();
  drawCommand.pushConstants = pushConstants.data();
  drawCommand.imageBarriers = imageBarriers.data();
  drawCommand.bufferBarriers = bufferBarriers.data();
  drawCommand.vertexBindings = vertexBindings.data();
  drawCommand.indexBindings = indexBindings.data();
  drawCommand.drawCalls = drawCalls.data();
  drawCommand.dispatchCalls = dispatchCalls.data();
  drawCommand.blitCalls = blitCalls.data();

  const uint32_t width = 1600, height = 1000;
  SDL_Init(SDL_INIT_VIDEO);
  auto window = SDL_CreateWindow("window", width, height, SDL_WINDOW_VULKAN);
  RendererSpecification rendererSpec{};
  rendererSpec.allowValidation = true;
  auto context = init(window, rendererSpec);

  //
  // DESCRIPTOR POOL
  //
  // TODO: Could go bindless, but I want to stay basic where possible
  DescriptorPool descriptorPool;
  DescriptorPoolSpecification descriptorPoolSpec{};
  descriptorPoolSpec.maxSets += 512;
  descriptorPoolSpec.uniformBufferCount = 256;
  descriptorPoolSpec.combinedSamplerCount = 256;
  descriptorPoolSpec.sampledImageCount = 256;
  descriptorPoolSpec.storageImageCount = 256;
  descriptorPool = createDescriptorPool(context, descriptorPoolSpec);

  //
  // JOB MANAGER
  //
  JobManager jobManager;
  initJobManager(jobManager, context);

  //
  // SCENE
  //
  SceneSpecification sceneSpec{};
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
  // MODELS
  //
  {
    AssimpLoadJob sponzaLoad{};
    sponzaLoad.file = argv[1];
    sponzaLoad.rotateZUp = true;
    sponzaLoad.scale = 1.f;
    sponzaLoad.descriptorPool = descriptorPool;
    sponzaLoad.sampler = scene.sampler;
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
  deferredPipelineSpec.shadowMapResolution = 2048;
  deferredPipelineSpec.shadowMapPixelFormat = PixelFormat::UNORM16_D;
  deferredPipelineSpec.colorPixelFormat = colorPixelFormat;
  deferredPipelineSpec.positionPixelFormat = positionPixelFormat;
  deferredPipelineSpec.depthPixelFormat = depthPixelFormat;
  deferredPipelineSpec.descriptorPool = descriptorPool;
  deferredPipelineSpec.drawProcessor = drawProcessor;
  DeferredPipeline deferredPipeline =
      createDeferredPipeline(context, deferredPipelineSpec);

  GeometryPipelineSpecification geometryPipelineSpec{};
  geometryPipelineSpec.width = width;
  geometryPipelineSpec.height = height;
  geometryPipelineSpec.colorPixelFormat = colorPixelFormat;
  geometryPipelineSpec.depthPixelFormat = depthPixelFormat;
  geometryPipelineSpec.drawProcessor = drawProcessor;
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
  } drawnPipeline = DrawnPipeline::DEFERRED;
  float renderingStartSeconds = getTimeSeconds();
  while (not shouldEnd) {
    float timeSeconds = getTimeSeconds() - renderingStartSeconds;
    if (timeSeconds > seconds + 1) {
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
    DrawCommandIndexes indexes{};
    switch (drawnPipeline) {
    case DrawnPipeline::GEOMETRY:
      geometryPipelineDrawFrame(context, geometryPipeline, timeSeconds,
                                drawCommand, indexes, scene, drawTarget,
                                drawTargetMeta);
      break;
    case DrawnPipeline::DEFERRED:
      deferredPipelineDrawFrame(context, deferredPipeline, timeSeconds,
                                drawCommand, indexes, scene, drawTarget,
                                drawTargetMeta);
      break;
    default:
      break;
    }

    //
    // PRESENT
    //
    present(context, swapchain, drawTarget, drawTargetMeta, nullptr);
    ++frames;

    timeDelta = (getTimeSeconds() - renderingStartSeconds) - timeSeconds;
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
  shutdown(context);

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
