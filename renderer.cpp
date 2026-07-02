#include "renderer.h"

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <print>
#include <thread>

// Graphics pipeline: TOP_OF_PIPE → DRAW_INDIRECT → TOP_OF_PIPE → VERTEX_INPUT →
// VERTEX_SHADER → TESSELLATION → GEOMETRY_SHADER → RASTERIZATION →
// EARLY_FRAGMENT_TESTS →FRAGMENT_SHADER → LATE_FRAGMENT_TESTS →
// COLOR_ATTACHMENT_OUTPUT → BOTTOM_OF_PIPE

// Transfer Unit (Independent): VK_PIPELINE_STAGE_TRANSFER_BIT (DMA engine)

struct LContextT {
  SDL_Window *window;
  VkInstance instance;
  VkDebugUtilsMessengerEXT debugMessenger;
  VkSurfaceKHR surface;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
  uint32_t graphicsQueueFamilyIndex;
  uint32_t transferQueueFamilyIndex;
  VkQueue graphicsQueue;
  VkQueue transferQueue;
  VkCommandPool graphicsCommandPool;
  VkCommandPool transferCommandPool;
  bool allowValidation;
};

struct BufferT {
  VkBuffer buffer;
  VkDeviceMemory memory;
  size_t size;
  VkMemoryPropertyFlags memoryPropertyFlags;
};

struct ShaderT {
  VkShaderModule module;
};

struct PipelineLayoutT {
  VkPipelineLayout layout;
};

struct PipelineT {
  VkPipeline pipeline;
  VkPipelineLayout layout;
  bool isGraphics;
};

struct DescriptorSetLayoutT {
  VkDescriptorSetLayout descriptorSetLayout;
  ResourceType *types;
  uint32_t typeCount;
};

struct RenderPassT {
  Image *renderTargets;
  uint32_t renderTargetCount;
  Image depthTarget;
  VkRenderingAttachmentInfo *colorAttachmentInfos;
  VkRenderingAttachmentInfo depthAttachmentInfo;
  uint32_t width;
  uint32_t height;
};

struct ImageT {
  VkImage image;
  VkImageView imageView;
  VkDeviceMemory memory;
  VkFormat format;
  uint32_t width;
  uint32_t height;
  bool isDepth;
};

struct SamplerT {
  VkSampler sampler;
};

struct SwapchainT {
  VkSwapchainKHR swapchain;
  uint32_t width;
  uint32_t height;
  VkFormat format;
  VkImage *images;
  VkImageView *imageViews;
  uint32_t imageCount;
  VkCommandPool commandPool;
  VkCommandBuffer commandBuffer;
  VkQueue presentQueue;
  VkSemaphore blitSemaphore;
  VkSemaphore acquireSemaphore;
  uint64_t lastFrameTime;
  uint32_t stabilizeFramerate;
};

struct DescriptorPoolT {
  VkDescriptorPool descriptorPool;
  // TODO: Should have a chain of descriptor pools for unlimited capacity
};

struct DescriptorSetT {
  DescriptorPool pool;
  VkDescriptorSet descriptorSet;
};

struct DrawProcessorT {
  VkCommandPool commandPool;
  VkCommandBuffer primaryCommandBuffer;
  // TODO: Test and fix this
  VkCommandBuffer *secondaryCommandBuffers;
  bool *isRecorded;
  uint32_t secondaryCommandBufferCount;
};

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL
ValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                   VkDebugUtilsMessageTypeFlagsEXT messageType,
                   const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                   void *pUserData) {
  // Figuring out what severity should the message be.
  const char *sevStr;
  switch (messageSeverity) {
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    sevStr = "trace";
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    sevStr = "info";
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    sevStr = "warn";
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    sevStr = "error";
    break;
  default:
    sevStr = "--";
    break;
  }

  if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    std::println("[{}] {}", sevStr, pCallbackData->pMessage);

  return VK_TRUE;
}

uint32_t findGraphicsQueueFamily(VkPhysicalDevice physicalDevice) {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           nullptr);

  VkQueueFamilyProperties *queueFamilies =
      new VkQueueFamilyProperties[queueFamilyCount];
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           queueFamilies);

  uint32_t graphicsFamily = UINT32_MAX;
  for (uint32_t i = 0; i < queueFamilyCount; i++) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      graphicsFamily = i;
      break;
    }
  }

  delete[] queueFamilies;
  return graphicsFamily;
}

uint32_t findTransferQueueFamily(VkPhysicalDevice physicalDevice) {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           nullptr);

  VkQueueFamilyProperties *queueFamilies =
      new VkQueueFamilyProperties[queueFamilyCount];
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           queueFamilies);

  uint32_t transferFamily = UINT32_MAX;
  for (uint32_t i = 0; i < queueFamilyCount; i++) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
        not(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      transferFamily = i;
      break;
    }
  }

  delete[] queueFamilies;
  return transferFamily;
}

uint32_t findPresentQueueFamily(VkPhysicalDevice physicalDevice,
                                VkSurfaceKHR surface) {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           nullptr);

  VkQueueFamilyProperties *queueFamilies = (VkQueueFamilyProperties *)malloc(
      queueFamilyCount * sizeof(VkQueueFamilyProperties));
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           queueFamilies);

  uint32_t presentFamily = UINT32_MAX;
  for (uint32_t i = 0; i < queueFamilyCount; i++) {
    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface,
                                         &presentSupport);
    if (presentSupport) {
      presentFamily = i;
      break;
    }
  }

  free(queueFamilies);
  return presentFamily;
}

bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
  uint32_t graphicsFamily = findGraphicsQueueFamily(device);
  if (graphicsFamily == UINT32_MAX) {
    return false;
  }

  // Check present support
  VkBool32 presentSupport = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(device, graphicsFamily, surface,
                                       &presentSupport);

  return presentSupport == VK_TRUE;
}

VkSurfaceFormatKHR chooseSwapchainFormat(VkPhysicalDevice physicalDevice,
                                         VkSurfaceKHR surface) {
  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       nullptr);

  VkSurfaceFormatKHR *formats =
      (VkSurfaceFormatKHR *)malloc(formatCount * sizeof(VkSurfaceFormatKHR));
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       formats);

  VkSurfaceFormatKHR chosenFormat = formats[0];

  // Prefer sRGB format
  for (uint32_t i = 0; i < formatCount; i++) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
        formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      chosenFormat = formats[i];
      break;
    }
  }

  free(formats);
  return chosenFormat;
}

VkPresentModeKHR chooseSwapchainPresentMode(VkPhysicalDevice physicalDevice,
                                            VkSurfaceKHR surface, bool vSync) {
  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                            &presentModeCount, nullptr);

  VkPresentModeKHR *presentModes = new VkPresentModeKHR[presentModeCount];
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                            &presentModeCount, presentModes);

  // Always available (double-buffering, vsynced)
  VkPresentModeKHR chosenMode = VK_PRESENT_MODE_FIFO_KHR;

  for (uint32_t i = 0; i < presentModeCount; i++) {
    if (vSync) {
      if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
        // Triple buffering
        chosenMode = VK_PRESENT_MODE_MAILBOX_KHR;
        break;
      }
    } else {
      if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
        chosenMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        break;
      }
    }
  }

  delete[] presentModes;
  return chosenMode;
}

VkExtent2D chooseSwapchainExtent(VkPhysicalDevice physicalDevice,
                                 VkSurfaceKHR surface, int windowWidth,
                                 int windowHeight) {
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                            &capabilities);

  if (capabilities.currentExtent.width != UINT32_MAX) {
    return capabilities.currentExtent;
  }

  VkExtent2D extent = {.width = (uint32_t)windowWidth,
                       .height = (uint32_t)windowHeight};

  extent.width = (extent.width < capabilities.minImageExtent.width)
                     ? capabilities.minImageExtent.width
                 : (extent.width > capabilities.maxImageExtent.width)
                     ? capabilities.maxImageExtent.width
                     : extent.width;

  extent.height = (extent.height < capabilities.minImageExtent.height)
                      ? capabilities.minImageExtent.height
                  : (extent.height > capabilities.maxImageExtent.height)
                      ? capabilities.maxImageExtent.height
                      : extent.height;

  return extent;
}

void destroyDebugMessenger(LContext context) {
  if (context->debugMessenger) {
    auto VkDestroyDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(context->instance,
                                  "vkDestroyDebugUtilsMessengerEXT"));
    if (VkDestroyDebugUtilsMessengerEXT && context->debugMessenger) {
      VkDestroyDebugUtilsMessengerEXT(context->instance,
                                      context->debugMessenger, nullptr);
    }
  }
}

} // namespace

LContext init(SDL_Window *window, const RendererSpecification &spec) {
  auto context = new LContextT;
  context->window = window;
  context->allowValidation = spec.allowValidation;

  //
  // INSTANCE
  //
  uint32_t enabledExtensionsCount;
  auto enabledExtensionsSDL =
      SDL_Vulkan_GetInstanceExtensions(&enabledExtensionsCount);
  const char **enabledExtensions = new const char *[enabledExtensionsCount + 1];
  std::memcpy(enabledExtensions, enabledExtensionsSDL,
              enabledExtensionsCount * sizeof(const char *));
  // TODO: should verify that layers exist
  enabledExtensions[enabledExtensionsCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

  VkApplicationInfo applicationInfo{};
  applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  applicationInfo.pApplicationName = "renderer";
  applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  applicationInfo.pEngineName = "renderer";
  applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  applicationInfo.apiVersion = VK_API_VERSION_1_4;

  VkInstanceCreateInfo instanceInfo{};
  instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceInfo.pApplicationInfo = &applicationInfo;
  instanceInfo.enabledExtensionCount = enabledExtensionsCount + 1;
  instanceInfo.ppEnabledExtensionNames = enabledExtensions;

  if (spec.allowValidation) {
    const char *enabledLayers[] = {"VK_LAYER_KHRONOS_validation"};
    instanceInfo.enabledLayerCount = std::size(enabledLayers);
    instanceInfo.ppEnabledLayerNames = std::data(enabledLayers);
  } else {
    instanceInfo.enabledLayerCount = 0;
    instanceInfo.ppEnabledLayerNames = nullptr;
  }

  VkResult result =
      vkCreateInstance(&instanceInfo, nullptr, &context->instance);
  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to create instance: {}", (int)result);
    delete context;
    return nullptr;
  }

  delete[] enabledExtensions;

  //
  // DEBUG MESSENGER
  //
  context->debugMessenger = nullptr;
  if (spec.allowValidation) {
    auto VkCreateDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(context->instance,
                                  "vkCreateDebugUtilsMessengerEXT"));

    context->debugMessenger = VK_NULL_HANDLE;
    if (VkCreateDebugUtilsMessengerEXT) {
      // Finalizing debug messenger construction if the creation procedure could
      // be retrieved.
      VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{};
      debugUtilsMessengerCreateInfo.sType =
          VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      debugUtilsMessengerCreateInfo.messageSeverity =
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      debugUtilsMessengerCreateInfo.messageType =
          VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
      debugUtilsMessengerCreateInfo.pfnUserCallback = ValidationCallback;
      auto res = VkCreateDebugUtilsMessengerEXT(
          context->instance, &debugUtilsMessengerCreateInfo, nullptr,
          &context->debugMessenger);

      if (res != VK_SUCCESS) {
        std::println("[Vulkan] Failed to create debug messenger: {}",
                     (int)result);
        vkDestroyInstance(context->instance, nullptr);
        delete context;
        return nullptr;
      }
    }
  }

  //
  // SURFACE
  //
  if (!SDL_Vulkan_CreateSurface(window, context->instance, nullptr,
                                &context->surface)) {
    std::println("[Vulkan] Failed to create Vulkan surface from SDL window");
    destroyDebugMessenger(context);
    vkDestroyInstance(context->instance, nullptr);
    delete context;
    return nullptr;
  }

  //
  // PHYSICAL DEVICE
  //
  uint32_t physicalDeviceCount = 0;
  vkEnumeratePhysicalDevices(context->instance, &physicalDeviceCount, nullptr);

  if (physicalDeviceCount == 0) {
    std::println("[Vulkan] No physical devices found");
    SDL_Vulkan_DestroySurface(context->instance, context->surface, nullptr);
    destroyDebugMessenger(context);
    vkDestroyInstance(context->instance, nullptr);
    delete context;
    return nullptr;
  }

  VkPhysicalDevice *physicalDevices = new VkPhysicalDevice[physicalDeviceCount];
  vkEnumeratePhysicalDevices(context->instance, &physicalDeviceCount,
                             physicalDevices);

  context->physicalDevice = VK_NULL_HANDLE;
  for (uint32_t i = 0; i < physicalDeviceCount; i++) {
    if (isDeviceSuitable(physicalDevices[i], context->surface)) {
      context->physicalDevice = physicalDevices[i];
      break;
    }
  }

  delete[] physicalDevices;

  if (context->physicalDevice == VK_NULL_HANDLE) {
    std::println("[Vulkan] No suitable physical device found");
    SDL_Vulkan_DestroySurface(context->instance, context->surface, nullptr);
    destroyDebugMessenger(context);
    vkDestroyInstance(context->instance, nullptr);
    delete context;
    return nullptr;
  }

  //
  // DEVICE
  //
  context->graphicsQueueFamilyIndex =
      findGraphicsQueueFamily(context->physicalDevice);
  context->transferQueueFamilyIndex =
      findTransferQueueFamily(context->physicalDevice);
  // TODO: Handle not present

  float queuePriority = 1.0f;
  VkDeviceQueueCreateInfo queueInfos[2]{};

  // Graphics queue
  uint32_t queueCreateInfoCount = 2;
  queueInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueInfos[0].queueFamilyIndex = context->graphicsQueueFamilyIndex;
  queueInfos[0].queueCount = 1;
  queueInfos[0].pQueuePriorities = &queuePriority;

  queueInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueInfos[1].queueFamilyIndex = context->transferQueueFamilyIndex;
  queueInfos[1].queueCount = 1;
  queueInfos[1].pQueuePriorities = &queuePriority;

  const char *deviceExtensions[] = {"VK_KHR_dynamic_rendering",
                                    "VK_KHR_swapchain"};

  VkPhysicalDeviceFeatures deviceFeatures{};

  VkDeviceCreateInfo deviceInfo{};
  deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceInfo.queueCreateInfoCount = queueCreateInfoCount;
  deviceInfo.pQueueCreateInfos = queueInfos;
  deviceInfo.pEnabledFeatures = &deviceFeatures;
  deviceInfo.ppEnabledExtensionNames = std::data(deviceExtensions);
  deviceInfo.enabledExtensionCount = std::size(deviceExtensions);
  if (spec.allowValidation) {
    const char *deviceLayers[] = {"VK_LAYER_KHRONOS_validation"};
    deviceInfo.ppEnabledLayerNames = std::data(deviceLayers);
    deviceInfo.enabledLayerCount = std::size(deviceLayers);
  } else {
    deviceInfo.ppEnabledLayerNames = nullptr;
    deviceInfo.enabledLayerCount = 0;
  }

  VkPhysicalDeviceVulkan13Features features13{};
  features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  // TODO: Check support
  features13.synchronization2 = VK_TRUE;
  features13.dynamicRendering = VK_TRUE;
  deviceInfo.pNext = &features13;

  result = vkCreateDevice(context->physicalDevice, &deviceInfo, nullptr,
                          &context->device);
  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to create logical device: {}", (int)result);
    vkDestroyDevice(context->device, nullptr);
    SDL_Vulkan_DestroySurface(context->instance, context->surface, nullptr);
    destroyDebugMessenger(context);
    vkDestroyInstance(context->instance, nullptr);
    delete context;
    return nullptr;
  }

  //
  // QUEUE
  //
  vkGetDeviceQueue(context->device, context->graphicsQueueFamilyIndex, 0,
                   &context->graphicsQueue);
  vkGetDeviceQueue(context->device, context->transferQueueFamilyIndex, 0,
                   &context->transferQueue);

  //
  // COMMAND POOL
  //
  VkCommandPoolCreateInfo commandPoolInfo{};
  commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolInfo.queueFamilyIndex = context->graphicsQueueFamilyIndex;

  result = vkCreateCommandPool(context->device, &commandPoolInfo, nullptr,
                               &context->graphicsCommandPool);
  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to create graphics command pool: {}",
                 (int)result);
    vkDestroyDevice(context->device, nullptr);
    SDL_Vulkan_DestroySurface(context->instance, context->surface, nullptr);
    destroyDebugMessenger(context);
    vkDestroyInstance(context->instance, nullptr);
    delete context;
    return nullptr;
  }

  commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolInfo.queueFamilyIndex = context->transferQueueFamilyIndex;
  result = vkCreateCommandPool(context->device, &commandPoolInfo, nullptr,
                               &context->transferCommandPool);
  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to create transfer command pool: {}",
                 (int)result);
    vkDestroyCommandPool(context->device, context->transferCommandPool,
                         nullptr);
    vkDestroyDevice(context->device, nullptr);
    SDL_Vulkan_DestroySurface(context->instance, context->surface, nullptr);
    destroyDebugMessenger(context);
    vkDestroyInstance(context->instance, nullptr);
    delete context;
    return nullptr;
  }

  return context;
}

void shutdown(LContext context) {
  vkDestroyCommandPool(context->device, context->transferCommandPool, nullptr);
  vkDestroyCommandPool(context->device, context->graphicsCommandPool, nullptr);
  vkDestroyDevice(context->device, nullptr);
  SDL_Vulkan_DestroySurface(context->instance, context->surface, nullptr);
  destroyDebugMessenger(context);
  vkDestroyInstance(context->instance, nullptr);
  delete context;
}

namespace {

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  // Fallback
  return 0;
}

VkBufferUsageFlags getBufferUsage(BufferUsage usage) {
  return (VkBufferUsageFlags)usage;
}

VkMemoryPropertyFlags computeMemoryProperties(const BufferSpecification &spec) {
  // CPU-writable buffers need host-visible memory
  if (spec.source == BufferSource::CPU) {
    VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    // Frequent updates benefit from coherent memory to avoid flushes
    if (spec.frequency == BufferFrequency::DYNAMIC ||
        spec.frequency == BufferFrequency::STREAM) {
      flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    return flags;
  }

  // CPU-readable buffers benefit from cached memory
  if (spec.consumer == BufferConsumer::CPU) {
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
           VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  }

  // GPU-only buffers prefer device-local memory
  return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}

uint32_t determineOwningSrcQueueFamilyIndex(LContext context,
                                            QueueOwnership ownership) {
  if (ownership == QueueOwnership::GRAPHICS) {
    return context->graphicsQueueFamilyIndex;
  }
  if (ownership == QueueOwnership::TRANSFER) {
    return context->transferQueueFamilyIndex;
  }
  if (ownership == QueueOwnership::GRAPHICS_RELEASED_TO_TRANSFER) {
    return context->graphicsQueueFamilyIndex;
  }
  if (ownership == QueueOwnership::TRANSFER_RELEASED_TO_GRAPHICS) {
    return context->transferQueueFamilyIndex;
  }
  if (ownership == QueueOwnership::NONE) {
    return UINT32_MAX;
  }

  std::println("[Vulkan] Unsupported queue ownership, defaulting to NONE");
  return UINT32_MAX;
}

uint32_t determineOwningDstQueueFamilyIndex(LContext context,
                                            QueueOwnership ownership) {
  if (ownership == QueueOwnership::GRAPHICS) {
    return context->graphicsQueueFamilyIndex;
  }
  if (ownership == QueueOwnership::TRANSFER) {
    return context->transferQueueFamilyIndex;
  }
  if (ownership == QueueOwnership::GRAPHICS_RELEASED_TO_TRANSFER) {
    return context->transferQueueFamilyIndex;
  }
  if (ownership == QueueOwnership::TRANSFER_RELEASED_TO_GRAPHICS) {
    return context->graphicsQueueFamilyIndex;
  }
  if (ownership == QueueOwnership::NONE) {
    return UINT32_MAX;
  }

  std::println("[Vulkan] Unsupported queue ownership, defaulting to NONE");
  return UINT32_MAX;
}

inline VkPipelineStageFlagBits2 getPipelineStages(PipelineStage stage) {
  return (VkPipelineStageFlagBits2)stage;
}

inline VkAccessFlagBits2 getResourceAccess(ResourceAccess access) {
  return (VkAccessFlagBits2)access;
}

void createBufferBarrier(LContext context, VkCommandBuffer commandBuffer,
                         Buffer buffer, const BufferMetadata &fromMeta,
                         const BufferMetadata &toMeta,
                         VkBufferMemoryBarrier2 &barrier) {
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
  barrier.pNext = nullptr;
  barrier.srcStageMask = getPipelineStages(fromMeta.stage);
  barrier.srcAccessMask = getResourceAccess(fromMeta.access);
  barrier.dstStageMask = getPipelineStages(toMeta.stage);
  barrier.dstAccessMask = getResourceAccess(toMeta.access);
  barrier.buffer = buffer->buffer;
  barrier.srcQueueFamilyIndex =
      determineOwningSrcQueueFamilyIndex(context, fromMeta.ownership);
  barrier.dstQueueFamilyIndex =
      determineOwningDstQueueFamilyIndex(context, toMeta.ownership);
  // TODO: Handle subresources properly
  barrier.offset = 0;
  barrier.size = buffer->size;
}

} // namespace

Buffer allocateBuffer(LContext context, BufferSpecification &spec) {
  Buffer buffer = new BufferT;
  buffer->size = spec.size;

  if (spec.outMeta) {
    spec.outMeta->stage = PipelineStage::NONE;
    spec.outMeta->access = ResourceAccess::NONE;
    spec.outMeta->ownership = spec.initialOwnership;
  }

  buffer->memoryPropertyFlags = computeMemoryProperties(spec);

  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = spec.size;
  bufferInfo.usage = getBufferUsage(spec.usage);
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(context->device, &bufferInfo, nullptr, &buffer->buffer) !=
      VK_SUCCESS) {
    delete buffer;
    return nullptr;
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(context->device, buffer->buffer,
                                &memRequirements);

  uint32_t memTypeIndex =
      findMemoryType(context->physicalDevice, memRequirements.memoryTypeBits,
                     buffer->memoryPropertyFlags);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = memTypeIndex;

  if (vkAllocateMemory(context->device, &allocInfo, nullptr, &buffer->memory) !=
      VK_SUCCESS) {
    vkDestroyBuffer(context->device, buffer->buffer, nullptr);
    delete buffer;
    return nullptr;
  }

  vkBindBufferMemory(context->device, buffer->buffer, buffer->memory, 0);

  return buffer;
}

void uploadBufferData(LContext context, Buffer buffer, const void *data,
                      size_t offset, size_t size) {
  void *mappedMemory = nullptr;
  vkMapMemory(context->device, buffer->memory, offset, size, 0, &mappedMemory);
  memcpy(mappedMemory, data, size);
  vkUnmapMemory(context->device, buffer->memory);

  // If not coherent, flush the writes
  if ((buffer->memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ==
      0) {
    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = buffer->memory;
    range.offset = offset;
    range.size = size;
    vkFlushMappedMemoryRanges(context->device, 1, &range);
  }
}

void copyBufferData(LContext context, const BufferCopySpecification &spec) {
  // Create and record command buffer
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = context->transferCommandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(context->device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  // TODO: This can be conditional
  VkBufferMemoryBarrier2 barriers[2];
  BufferMetadata sourceMeta{};
  sourceMeta.stage = PipelineStage::TRANSFER;
  sourceMeta.access = ResourceAccess::TRANSFER_READ;
  sourceMeta.ownership = QueueOwnership::TRANSFER;
  createBufferBarrier(context, commandBuffer, spec.source, spec.sourceMeta,
                      sourceMeta, barriers[0]);
  BufferMetadata destinationMeta{};
  destinationMeta.stage = PipelineStage::TRANSFER;
  destinationMeta.access = ResourceAccess::TRANSFER_WRITE;
  destinationMeta.ownership = QueueOwnership::TRANSFER;
  createBufferBarrier(context, commandBuffer, spec.destination,
                      spec.destinationMeta, destinationMeta, barriers[1]);
  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependencyInfo.pBufferMemoryBarriers = barriers;
  dependencyInfo.bufferMemoryBarrierCount = 2;
  vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

  VkBufferCopy region{};
  region.srcOffset = spec.sourceOffset;
  region.dstOffset = spec.destinationOffset;
  region.size = spec.size;

  vkCmdCopyBuffer(commandBuffer, spec.source->buffer, spec.destination->buffer,
                  1, &region);

  // TODO: We assume the buffer will be used in graphics operations now and the
  // staging buffer will stay transfer-owned, this could be relaxed using a copy
  // spec
  BufferMetadata finalDestinationMeta{};
  finalDestinationMeta.stage = PipelineStage::BOTTOM_OF_PIPE;
  finalDestinationMeta.access = ResourceAccess::NONE;
  finalDestinationMeta.ownership =
      QueueOwnership::TRANSFER_RELEASED_TO_GRAPHICS;
  createBufferBarrier(context, commandBuffer, spec.destination, destinationMeta,
                      finalDestinationMeta, barriers[0]);
  dependencyInfo.bufferMemoryBarrierCount = 1;
  vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

  vkEndCommandBuffer(commandBuffer);

  if (spec.outSourceMeta) {
    *spec.outSourceMeta = sourceMeta;
  }
  if (spec.outDestinationMeta) {
    *spec.outDestinationMeta = destinationMeta;
  }

  // Submit and wait
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(context->transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(context->transferQueue);

  vkFreeCommandBuffers(context->device, context->transferCommandPool, 1,
                       &commandBuffer);
}

void destroyBuffer(LContext context, Buffer buffer) {
  vkDestroyBuffer(context->device, buffer->buffer, nullptr);
  vkFreeMemory(context->device, buffer->memory, nullptr);
  delete buffer;
}

namespace {

VkFormat getVulkanFormat(PixelFormat pixelFormat) {
  // TODO: This has grown quite a bit, could directly map the values in my enum
  switch (pixelFormat) {
  case PixelFormat::UINT8_RGBA_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  case PixelFormat::UINT8_BGRA_SRGB:
    return VK_FORMAT_B8G8R8A8_SRGB;
  case PixelFormat::UINT8_RGBA:
    return VK_FORMAT_R8G8B8A8_UINT;
  case PixelFormat::UINT8_BGRA:
    return VK_FORMAT_B8G8R8A8_UINT;
  case PixelFormat::UINT8_RGB_SRGB:
    return VK_FORMAT_R8G8B8_SRGB;
  case PixelFormat::UINT8_BGR_SRGB:
    return VK_FORMAT_B8G8R8_SRGB;
  case PixelFormat::UINT8_RGB:
    return VK_FORMAT_R8G8B8_UINT;
  case PixelFormat::UINT8_BGR:
    return VK_FORMAT_B8G8R8_UINT;
  case PixelFormat::UNORM8_RGBA:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case PixelFormat::UNORM8_BGRA:
    return VK_FORMAT_B8G8R8A8_UNORM;
  case PixelFormat::UNORM8_RGB:
    return VK_FORMAT_R8G8B8_UNORM;
  case PixelFormat::UNORM8_BGR:
    return VK_FORMAT_B8G8R8_UNORM;
  case PixelFormat::UINT8_R_SRGB:
    return VK_FORMAT_R8_SRGB;
  case PixelFormat::UINT8_R:
    return VK_FORMAT_R8_UINT;
  case PixelFormat::UNORM8_R:
    return VK_FORMAT_R8_UNORM;
  case PixelFormat::UINT16_RGBA:
    return VK_FORMAT_R16G16B16A16_UINT;
  case PixelFormat::UINT16_RGB:
    return VK_FORMAT_R16G16B16_UINT;
  case PixelFormat::UNORM16_RGBA:
    return VK_FORMAT_R16G16B16A16_UNORM;
  case PixelFormat::UNORM16_RGB:
    return VK_FORMAT_R16G16B16_UNORM;
  case PixelFormat::UINT16_R:
    return VK_FORMAT_R16_UINT;
  case PixelFormat::UNORM16_R:
    return VK_FORMAT_R16_UNORM;
  case PixelFormat::UINT32_RGBA:
    return VK_FORMAT_R32G32B32A32_UINT;
  case PixelFormat::UINT32_RGB:
    return VK_FORMAT_R32G32B32_UINT;
  case PixelFormat::UINT32_R:
    return VK_FORMAT_R32_UINT;
  case PixelFormat::FLOAT32_RGBA:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case PixelFormat::FLOAT32_RGB:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case PixelFormat::FLOAT32_R:
    return VK_FORMAT_R32_SFLOAT;
  case PixelFormat::FLOAT16_RGBA:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case PixelFormat::FLOAT16_RGB:
    return VK_FORMAT_R16G16B16_SFLOAT;
  case PixelFormat::FLOAT16_R:
    return VK_FORMAT_R16_SFLOAT;
  case PixelFormat::UNORM16_D:
    return VK_FORMAT_D16_UNORM;
  case PixelFormat::FLOAT32_D:
    return VK_FORMAT_D32_SFLOAT;
  }

  std::println("[Vulkan] Unknown pixel format, defaulting to UINT8_RGBA_SRGB");
  return VK_FORMAT_R8G8B8A8_SRGB;
}

VkImageTiling getImageTiling(ImageTiling imageTiling) {
  switch (imageTiling) {
  case ImageTiling::OPTIMAL:
    return VK_IMAGE_TILING_OPTIMAL;
  case ImageTiling::LINEAR:
    return VK_IMAGE_TILING_LINEAR;
  }

  std::println("[Vulkan] Unknown image tiling, defaulting to OPTIMAL");
  return VK_IMAGE_TILING_OPTIMAL;
}

VkFrontFace getFrontFace(FrontFaceWind wind) {
  if (wind == FrontFaceWind::CLOCKWISE)
    return VK_FRONT_FACE_CLOCKWISE;
  return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

bool isDepthFormat(PixelFormat pixelFormat) {
  return (pixelFormat == PixelFormat::UNORM16_D ||
          pixelFormat == PixelFormat::FLOAT32_D);
}

inline VkImageUsageFlags getImageUsageFlags(ImageUsage imageUsage) {
  return (VkImageUsageFlags)imageUsage;
}

VkImageLayout getImageLayout(ImageLayout layout) {
  switch (layout) {
  case ImageLayout::UNDEFINED:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  case ImageLayout::GENERAL:
    return VK_IMAGE_LAYOUT_GENERAL;
  case ImageLayout::COLOR_RENDER_TARGET:
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  case ImageLayout::DEPTH_RENDER_TARGET:
    return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  case ImageLayout::SHADER_READ_ONLY:
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  case ImageLayout::TRANSFER_SRC:
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  case ImageLayout::TRANSFER_DST:
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  case ImageLayout::PRESENT:
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  }

  std::println("[Vulkan] Unsupported image layout, defaulting to UNDEFINED");
  return VK_IMAGE_LAYOUT_UNDEFINED;
}

void createImageBarrier(LContext context, VkCommandBuffer commandBuffer,
                        Image image, const ImageMetadata &fromMeta,
                        const ImageMetadata &toMeta,
                        VkImageMemoryBarrier2 &barrier) {
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.pNext = nullptr;
  barrier.oldLayout = getImageLayout(fromMeta.layout);
  barrier.newLayout = getImageLayout(toMeta.layout);
  barrier.srcStageMask = getPipelineStages(fromMeta.stage);
  barrier.srcAccessMask = getResourceAccess(fromMeta.access);
  barrier.dstStageMask = getPipelineStages(toMeta.stage);
  barrier.dstAccessMask = getResourceAccess(toMeta.access);
  barrier.srcQueueFamilyIndex =
      determineOwningSrcQueueFamilyIndex(context, fromMeta.ownership);
  barrier.dstQueueFamilyIndex =
      determineOwningDstQueueFamilyIndex(context, toMeta.ownership);
  barrier.image = image->image;
  if (image->isDepth) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  // TODO: Handle subresources properly
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
}

void blitImage(VkCommandBuffer commandBuffer, VkImage srcImage,
               VkImage dstImage, uint32_t srcWidth, uint32_t srcHeight,
               uint32_t dstWidth, uint32_t dstHeight, VkFilter filter) {
  VkImageBlit blit{};
  blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit.srcSubresource.layerCount = 1;
  blit.srcOffsets[1] = {(int32_t)srcWidth, (int32_t)srcHeight, 1};

  blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit.dstSubresource.layerCount = 1;
  blit.dstOffsets[1] = {(int32_t)dstWidth, (int32_t)dstHeight, 1};

  vkCmdBlitImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                 filter);
}

void copyImage(VkCommandBuffer commandBuffer, VkImage srcImage,
               VkImage dstImage, uint32_t width, uint32_t height) {
  VkImageCopy copy{};
  copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.srcSubresource.layerCount = 1;
  copy.srcOffset = {0, 0, 0};

  copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.dstSubresource.layerCount = 1;
  copy.dstOffset = {0, 0, 0};

  copy.extent = {width, height, 1};

  vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
}

void copyImage(VkCommandBuffer commandBuffer, VkImage srcImage,
               VkImage dstImage, uint32_t srcX, uint32_t srcY, uint32_t dstX,
               uint32_t dstY, uint32_t width, uint32_t height) {
  VkImageCopy copy{};
  copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.srcSubresource.layerCount = 1;
  copy.srcOffset = {(int32_t)srcX, (int32_t)srcY, 0};

  copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.dstSubresource.layerCount = 1;
  copy.dstOffset = {(int32_t)dstX, (int32_t)dstY, 0};

  copy.extent = {width, height, 1};

  vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
}

VkFilter getVulkanFilter(SampleFilter filter) {
  switch (filter) {
  case SampleFilter::NEAREST:
    return VK_FILTER_NEAREST;
  case SampleFilter::LINEAR:
    return VK_FILTER_LINEAR;
  }

  std::println("[Vulkan] Unsupported filter, defaulting to NEAREST");
  return VK_FILTER_NEAREST;
}

VkSamplerAddressMode getAddressMode(SampleAddressing addressing) {
  switch (addressing) {
  case SampleAddressing::REPEAT:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  case SampleAddressing::MIRRORED_REPEAT:
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  case SampleAddressing::CLAMP_TO_EDGE:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  case SampleAddressing::CLAMP_TO_BORDER:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  case SampleAddressing::MIRROR_CLAMP_TO_EDGE:
    return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
  }

  std::println("[Vulkan] Unsupported addressing, defaulting to REPEAT");
  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

} // namespace

Image createImage(LContext context, const ImageSpecification &spec) {
  auto image = new ImageT;
  image->width = spec.width;
  image->height = spec.height;
  image->format = getVulkanFormat(spec.pixelFormat);
  if (spec.outMeta) {
    spec.outMeta->layout = spec.initialLayout;
    spec.outMeta->ownership = QueueOwnership::NONE;
    spec.outMeta->stage = PipelineStage::NONE;
    spec.outMeta->access = ResourceAccess::NONE;
  }

  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = spec.width;
  imageInfo.extent.height = spec.height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = image->format;
  imageInfo.tiling = getImageTiling(spec.tiling);
  imageInfo.initialLayout = getImageLayout(spec.initialLayout);
  imageInfo.usage = getImageUsageFlags(spec.usage);
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  auto result =
      vkCreateImage(context->device, &imageInfo, nullptr, &image->image);
  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to create render target image: {}",
                 (int)result);
    delete image;
    return nullptr;
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(context->device, image->image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      findMemoryType(context->physicalDevice, memRequirements.memoryTypeBits,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  result =
      vkAllocateMemory(context->device, &allocInfo, nullptr, &image->memory);
  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to allocate render target memory: {}",
                 (int)result);
    vkDestroyImage(context->device, image->image, nullptr);
    delete image;
    return nullptr;
  }

  vkBindImageMemory(context->device, image->image, image->memory, 0);

  // Create VkImageView
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image->image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = image->format;
  if (isDepthFormat(spec.pixelFormat)) {
    image->isDepth = true;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else {
    image->isDepth = false;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  result =
      vkCreateImageView(context->device, &viewInfo, nullptr, &image->imageView);
  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to create render target image view: {}",
                 (int)result);
    vkFreeMemory(context->device, image->memory, nullptr);
    vkDestroyImage(context->device, image->image, nullptr);
    delete image;
    return nullptr;
  }

  return image;
}

void copyImageData(LContext context, const ImageDataCopySpecification &spec) {
  // Begin command buffer recording
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = context->transferCommandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(context->device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  // Transition image layout to transfer destination
  VkBufferMemoryBarrier2 bufferBarrier{};
  VkImageMemoryBarrier2 imageBarrier{};
  BufferMetadata stagingMeta{};
  stagingMeta.stage = PipelineStage::TRANSFER;
  stagingMeta.access = ResourceAccess::TRANSFER_READ;
  stagingMeta.ownership = QueueOwnership::TRANSFER;
  createBufferBarrier(context, commandBuffer, spec.stagingBuffer,
                      spec.stagingBufferMeta, stagingMeta, bufferBarrier);
  if (spec.outStaingBufferMeta) {
    *spec.outStaingBufferMeta = stagingMeta;
  }
  ImageMetadata imageMeta = spec.imageMeta;
  if (imageMeta.ownership == QueueOwnership::NONE) {
    // TODO: More validation here
    imageMeta.ownership = QueueOwnership::TRANSFER;
  }
  ImageMetadata transferMeta;
  transferMeta.layout = ImageLayout::TRANSFER_DST;
  transferMeta.ownership = QueueOwnership::TRANSFER;
  transferMeta.stage = PipelineStage::TRANSFER;
  transferMeta.access = ResourceAccess::TRANSFER_WRITE;
  createImageBarrier(context, commandBuffer, spec.image, imageMeta,
                     transferMeta, imageBarrier);
  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependencyInfo.pBufferMemoryBarriers = &bufferBarrier;
  dependencyInfo.bufferMemoryBarrierCount = 1;
  dependencyInfo.pImageMemoryBarriers = &imageBarrier;
  dependencyInfo.imageMemoryBarrierCount = 1;
  vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

  // Copy buffer to image
  VkBufferImageCopy region{};
  region.bufferOffset = spec.bufferOffset;
  region.bufferRowLength = 0;   // width
  region.bufferImageHeight = 0; // height
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {spec.width, spec.height, 1};

  vkCmdCopyBufferToImage(commandBuffer, spec.stagingBuffer->buffer,
                         spec.image->image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  // TODO: We assume ownership release
  ImageMetadata outMeta;
  outMeta.layout = ImageLayout::TRANSFER_DST;
  outMeta.ownership = QueueOwnership::TRANSFER_RELEASED_TO_GRAPHICS;
  outMeta.stage = PipelineStage::BOTTOM_OF_PIPE;
  outMeta.access = ResourceAccess::NONE;
  createImageBarrier(context, commandBuffer, spec.image, transferMeta, outMeta,
                     imageBarrier);
  if (spec.outImageMeta) {
    *spec.outImageMeta = outMeta;
  }
  dependencyInfo.pBufferMemoryBarriers = nullptr;
  dependencyInfo.bufferMemoryBarrierCount = 0;
  vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

  vkEndCommandBuffer(commandBuffer);

  // Submit command buffer
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(context->transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(context->transferQueue);

  // Cleanup
  vkFreeCommandBuffers(context->device, context->transferCommandPool, 1,
                       &commandBuffer);
}

void copyImage(LContext context, const ImageCopySpecification &spec) {
  // TODO: Reimplement this with transfer queue after solving the ownership
  // problem

  // Begin command buffer recording
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = context->graphicsCommandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(context->device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  ImageMetadata transferMeta;
  transferMeta.layout = ImageLayout::TRANSFER_SRC;
  transferMeta.ownership = QueueOwnership::GRAPHICS;
  transferMeta.stage = PipelineStage::TRANSFER;
  transferMeta.access = ResourceAccess::TRANSFER_READ;

  VkImageMemoryBarrier2 barriers[2];
  uint32_t barrierCount = 0;
  if (spec.sourceMeta.layout != ImageLayout::TRANSFER_SRC) {
    createImageBarrier(context, commandBuffer, spec.source, spec.sourceMeta,
                       transferMeta, barriers[0]);
    ++barrierCount;
  }

  if (spec.outSourceMeta) {
    *spec.outSourceMeta = transferMeta;
  }

  transferMeta.layout = ImageLayout::TRANSFER_DST;
  transferMeta.access = ResourceAccess::TRANSFER_WRITE;
  if (spec.destinationMeta.layout != ImageLayout::TRANSFER_DST) {
    createImageBarrier(context, commandBuffer, spec.destination,
                       spec.destinationMeta, transferMeta,
                       barriers[barrierCount++]);
  }

  if (spec.outDestinationMeta) {
    *spec.outDestinationMeta = transferMeta;
  }

  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  if (barrierCount > 0) {
    dependencyInfo.pImageMemoryBarriers = barriers;
    dependencyInfo.imageMemoryBarrierCount = barrierCount;
    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
  }

  copyImage(commandBuffer, spec.source->image, spec.destination->image,
            spec.srcX, spec.srcY, spec.dstX, spec.dstY, spec.width,
            spec.height);

  // TODO: No barrier here, we don't necessarily know what happens next to the
  // image and there is no queue ownership transfer

  vkEndCommandBuffer(commandBuffer);

  // Submit command buffer
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(context->graphicsQueue);

  // Cleanup
  vkFreeCommandBuffers(context->device, context->graphicsCommandPool, 1,
                       &commandBuffer);
}

void blitImage(LContext context, const ImageBlitSpecification &spec) {
  // Begin command buffer recording
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = context->graphicsCommandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(context->device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  ImageMetadata transferMeta;
  transferMeta.layout = ImageLayout::TRANSFER_SRC;
  transferMeta.ownership = QueueOwnership::GRAPHICS;
  transferMeta.stage = PipelineStage::TRANSFER;
  transferMeta.access = ResourceAccess::TRANSFER_READ;

  VkImageMemoryBarrier2 barriers[2];
  uint32_t barrierCount = 0;
  if (spec.sourceMeta.layout != ImageLayout::TRANSFER_SRC) {
    createImageBarrier(context, commandBuffer, spec.source, spec.sourceMeta,
                       transferMeta, barriers[0]);
    ++barrierCount;
  }

  if (spec.outSourceMeta) {
    *spec.outSourceMeta = transferMeta;
  }

  transferMeta.layout = ImageLayout::TRANSFER_DST;
  transferMeta.access = ResourceAccess::TRANSFER_WRITE;

  // Transition destination to transfer destination if needed
  if (spec.destinationMeta.layout != ImageLayout::TRANSFER_DST) {
    createImageBarrier(context, commandBuffer, spec.destination,
                       spec.destinationMeta, transferMeta,
                       barriers[barrierCount++]);
  }

  if (spec.outDestinationMeta) {
    *spec.outDestinationMeta = transferMeta;
  }

  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  if (barrierCount > 0) {
    dependencyInfo.pImageMemoryBarriers = barriers;
    dependencyInfo.imageMemoryBarrierCount = barrierCount;
    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
  }

  blitImage(commandBuffer, spec.source->image, spec.destination->image,
            spec.source->width, spec.source->height, spec.destination->width,
            spec.destination->height, getVulkanFilter(spec.filter));

  // TODO: No barrier here, we don't necessarily know what happens next to the
  // image and there is no queue ownership transfer

  vkEndCommandBuffer(commandBuffer);

  // Submit command buffer
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(context->graphicsQueue);

  // Cleanup
  vkFreeCommandBuffers(context->device, context->graphicsCommandPool, 1,
                       &commandBuffer);
}

void destroyImage(LContext context, Image image) {
  vkDestroyImageView(context->device, image->imageView, nullptr);
  vkDestroyImage(context->device, image->image, nullptr);
  vkFreeMemory(context->device, image->memory, nullptr);
  delete image;
}

void graphicsAcquireImage(LContext context, Image image, ImageMetadata meta,
                          ImageMetadata *outMeta) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = context->graphicsCommandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(context->device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  ImageMetadata resultMeta;
  resultMeta.layout = ImageLayout::SHADER_READ_ONLY;
  resultMeta.ownership = QueueOwnership::GRAPHICS;
  resultMeta.stage = PipelineStage::FRAGMENT_SHADER;
  resultMeta.access = ResourceAccess::SHADER_READ;

  VkImageMemoryBarrier2 imageBarrier{};
  imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  createImageBarrier(context, commandBuffer, image, meta, resultMeta,
                     imageBarrier);
  if (outMeta) {
    *outMeta = resultMeta;
  }
  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependencyInfo.pImageMemoryBarriers = &imageBarrier;
  dependencyInfo.imageMemoryBarrierCount = 1;
  vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

  vkEndCommandBuffer(commandBuffer);

  // Submit command buffer
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(context->graphicsQueue);

  // Cleanup
  vkFreeCommandBuffers(context->device, context->graphicsCommandPool, 1,
                       &commandBuffer);
}

Sampler createSampler(LContext context, const SamplerSpecification &spec) {
  auto sampler = new SamplerT;

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.minFilter = getVulkanFilter(spec.minFilter);
  samplerInfo.magFilter = getVulkanFilter(spec.magFilter);
  samplerInfo.addressModeU = getAddressMode(spec.addressingU);
  samplerInfo.addressModeV = getAddressMode(spec.addressingV);
  samplerInfo.addressModeW = getAddressMode(spec.addressingW);
  vkCreateSampler(context->device, &samplerInfo, nullptr, &sampler->sampler);

  return sampler;
}

void destroySampler(LContext context, Sampler sampler) {
  vkDestroySampler(context->device, sampler->sampler, nullptr);
  delete sampler;
}

Shader loadSPIRVShader(LContext context, uint8_t bytes[], size_t size) {
  auto shader = new ShaderT;
  VkShaderModuleCreateInfo shaderModuleInfo{};
  shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderModuleInfo.pCode = (uint32_t *)bytes;
  shaderModuleInfo.codeSize = size;
  VkResult result = vkCreateShaderModule(context->device, &shaderModuleInfo,
                                         nullptr, &shader->module);
  return shader;
}

void destroyShader(LContext context, Shader shader) {
  vkDestroyShaderModule(context->device, shader->module, nullptr);
  delete shader;
}

namespace {

VkDescriptorType getDescriptorType(ResourceType type) {
  switch (type) {
  case ResourceType::UNIFORM_BUFFER:
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  case ResourceType::SAMPLER:
    return VK_DESCRIPTOR_TYPE_SAMPLER;
  case ResourceType::SAMPLED_IMAGE:
    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  case ResourceType::STORAGE_IMAGE:
    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  case ResourceType::COMBINED_SAMPLER:
    return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  case ResourceType::STORAGE_BUFFER:
    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  case ResourceType::STORAGE_TEXEL_BUFFER:
    return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
  }

  std::println("[Vulkan] Unknown uniform type, defaulting to UNIFORM_BUFFER");
  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

VkShaderStageFlags getShaderStageFlags(ShaderVisibility visibility) {
  return (VkShaderStageFlags)visibility;
}

VkFormat getVkVertexFormat(VertexFormat format) {
  // TODO: make my enum values equivalent
  switch (format) {
  case VertexFormat::FLOAT:
    return VK_FORMAT_R32_SFLOAT;
  case VertexFormat::FLOAT2:
    return VK_FORMAT_R32G32_SFLOAT;
  case VertexFormat::FLOAT3:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case VertexFormat::FLOAT4:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case VertexFormat::INT:
    return VK_FORMAT_R32_SINT;
  case VertexFormat::INT2:
    return VK_FORMAT_R32G32_SINT;
  case VertexFormat::INT3:
    return VK_FORMAT_R32G32B32_SINT;
  case VertexFormat::INT4:
    return VK_FORMAT_R32G32B32A32_SINT;
  case VertexFormat::UINT:
    return VK_FORMAT_R32_UINT;
  case VertexFormat::UINT2:
    return VK_FORMAT_R32G32_UINT;
  case VertexFormat::UINT3:
    return VK_FORMAT_R32G32B32_UINT;
  case VertexFormat::UINT4:
    return VK_FORMAT_R32G32B32A32_UINT;
  default:
    return VK_FORMAT_UNDEFINED;
  }
}

VkVertexInputRate getVkInputRate(VertexInputRate rate) {
  return (rate == VertexInputRate::VERTEX) ? VK_VERTEX_INPUT_RATE_VERTEX
                                           : VK_VERTEX_INPUT_RATE_INSTANCE;
}

VkPrimitiveTopology getPrimitiveTopology(DrawMode drawMode) {
  switch (drawMode) {
  case DrawMode::TRIANGLES:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  case DrawMode::TRIANGLE_STRIP:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  }

  std::println("Unsupported draw mode, defaulting to TRIANGLES");
  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

VkCullModeFlags getCullMode(FaceCulling culling) {
  switch (culling) {
  case FaceCulling::NONE:
    return VK_CULL_MODE_NONE;
  case FaceCulling::BACK:
    return VK_CULL_MODE_BACK_BIT;
  case FaceCulling::FRONT:
    return VK_CULL_MODE_FRONT_BIT;
  }

  std::println("Unsupported face culling, defaulting to NONE");
  return VK_CULL_MODE_NONE;
}

VkBlendOp getBlendOp(BlendOp op) {
  switch (op) {
  case BlendOp::ADD:
    return VK_BLEND_OP_ADD;
  case BlendOp::SUBTRACT:
    return VK_BLEND_OP_SUBTRACT;
  case BlendOp::MIN:
    return VK_BLEND_OP_MIN;
  case BlendOp::MAX:
    return VK_BLEND_OP_MAX;
  }

  std::println("Unsupported blend op, defaulting to ADD");
  return VK_BLEND_OP_ADD;
}

VkBlendFactor getBlendFactor(BlendFactor factor) {
  return (VkBlendFactor)factor;
}

VkColorComponentFlags getColorComponents(ColorComponent components) {
  return (VkColorComponentFlags)components;
}

} // namespace

PipelineLayout createPipelineLayout(LContext context, DescriptorSetLayout *sets,
                                    uint32_t setCount,
                                    PushConstant *pushConstants,
                                    uint32_t constantCount) {
  if (setCount > 4) {
    std::println(
        "[Vulkan] Cannot bind more than 4 descriptor sets at one time");
    return nullptr;
  }
  if (constantCount > 128) {
    std::println("[Vulkan] Cannot request more than 128 push constant ranges "
                 "at one time");
    return nullptr;
  }
  auto pipelineLayout = new PipelineLayoutT;

  VkDescriptorSetLayout layouts[4];
  for (int i = 0; i < setCount; ++i) {
    layouts[i] = sets[i]->descriptorSetLayout;
  }

  // TODO: Base push constants on available push constant memory
  VkPushConstantRange ranges[128];
  for (int i = 0; i < constantCount; ++i) {
    ranges[i].stageFlags = getShaderStageFlags(pushConstants[i].visibility);
    ranges[i].offset = pushConstants[i].offset;
    ranges[i].size = pushConstants[i].size;
  }

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.pSetLayouts = setCount > 0 ? layouts : nullptr;
  pipelineLayoutInfo.setLayoutCount = setCount;
  pipelineLayoutInfo.pPushConstantRanges = constantCount > 0 ? ranges : nullptr;
  pipelineLayoutInfo.pushConstantRangeCount = constantCount;

  vkCreatePipelineLayout(context->device, &pipelineLayoutInfo, nullptr,
                         &pipelineLayout->layout);

  return pipelineLayout;
}

void destroyPipelineLayout(LContext context, PipelineLayout pipelineLayout) {
  vkDestroyPipelineLayout(context->device, pipelineLayout->layout, nullptr);
  delete pipelineLayout;
}

Pipeline createGraphicsPipeline(LContext context,
                                const GraphicsPipelineSpecification &spec) {
  auto pipeline = new PipelineT;
  VkDevice device = context->device;

  pipeline->pipeline = VK_NULL_HANDLE;
  pipeline->layout = spec.pipelineLayout->layout;
  pipeline->isGraphics = true;

  //
  // SHADER STAGES
  //
  VkPipelineShaderStageCreateInfo shaderStages[2]{};

  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = spec.vertexShader->module;
  shaderStages[0].pName = "main";

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = spec.fragmentShader->module;
  shaderStages[1].pName = "main";

  //
  // VERTEX INPUT STATE
  //
  VkVertexInputAttributeDescription *attributeDescs = nullptr;
  if (spec.attributeCount > 0) {
    attributeDescs = new VkVertexInputAttributeDescription[spec.attributeCount];

    for (uint32_t i = 0; i < spec.attributeCount; i++) {
      const VertexAttributeDescription &attr = spec.attributes[i];
      attributeDescs[i].location = attr.location;
      attributeDescs[i].binding = attr.binding;
      attributeDescs[i].format = getVkVertexFormat(attr.format);
      attributeDescs[i].offset = attr.offset;
    }
  }

  VkVertexInputBindingDescription *bindingDescs = nullptr;
  if (spec.vertexBindingCount > 0) {
    bindingDescs = new VkVertexInputBindingDescription[spec.vertexBindingCount];

    for (uint32_t i = 0; i < spec.vertexBindingCount; i++) {
      const VertexBufferBinding &binding = spec.vertexBindings[i];
      bindingDescs[i].binding = binding.bindingIndex;
      bindingDescs[i].stride = binding.stride;
      bindingDescs[i].inputRate = getVkInputRate(binding.inputRate);
    }
  }

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexAttributeDescriptionCount = spec.attributeCount;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescs;
  vertexInputInfo.vertexBindingDescriptionCount = spec.vertexBindingCount;
  vertexInputInfo.pVertexBindingDescriptions = bindingDescs;

  //
  // INPUT ASSEMBLY
  //
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
  inputAssemblyInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyInfo.topology = getPrimitiveTopology(spec.drawMode);
  inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

  //
  // VIEWPORT & SCISSOR
  //
  VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
  dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicStateInfo.dynamicStateCount = 0;
  dynamicStateInfo.pDynamicStates = nullptr;

  VkViewport viewport{};
  viewport.x = 0;
  viewport.width = spec.width;
  if (spec.flipViewportY) {
    viewport.y = spec.height;
    viewport.height = -(float)spec.height;
  } else {
    viewport.y = 0;
    viewport.height = spec.height;
  }
  viewport.minDepth = spec.depthMin;
  viewport.maxDepth = spec.depthMax;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = {spec.width, spec.height};

  VkPipelineViewportStateCreateInfo viewportStateInfo{};
  viewportStateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportStateInfo.viewportCount = 1;
  viewportStateInfo.scissorCount = 1;
  viewportStateInfo.pViewports = &viewport;
  viewportStateInfo.pScissors = &scissor;

  //
  // RASTERIZATION
  //
  VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
  rasterizationInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizationInfo.depthClampEnable = VK_FALSE;
  rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
  rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizationInfo.cullMode = getCullMode(spec.faceCulling);
  rasterizationInfo.frontFace = getFrontFace(spec.frontFaceWind);
  rasterizationInfo.depthBiasEnable = VK_FALSE;
  rasterizationInfo.lineWidth = 1.0f;

  //
  // MULTISAMPLE
  //
  VkPipelineMultisampleStateCreateInfo multisampleInfo{};
  multisampleInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampleInfo.sampleShadingEnable = VK_FALSE;
  multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  //
  // DEPTH & STENCIL
  //
  VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
  depthStencilInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilInfo.depthTestEnable = spec.enableDepthTest ? VK_TRUE : VK_FALSE;
  depthStencilInfo.depthWriteEnable =
      spec.enableDepthWrite ? VK_TRUE : VK_FALSE;
  depthStencilInfo.depthCompareOp = VK_COMPARE_OP_GREATER;
  depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
  depthStencilInfo.stencilTestEnable = VK_FALSE;
  depthStencilInfo.minDepthBounds = 1.0f;
  depthStencilInfo.maxDepthBounds = 0.0f;

  //
  // COLOR BLENDING
  //
  VkPipelineColorBlendAttachmentState *colorBlendAttachments = nullptr;
  if (spec.renderTargetCount > 0) {
    colorBlendAttachments =
        new VkPipelineColorBlendAttachmentState[spec.renderTargetCount]{};
    for (int i = 0; i < spec.renderTargetCount; ++i) {
      colorBlendAttachments[i].colorWriteMask =
          getColorComponents(spec.renderTargetBlends[i].colorWriteMask);

      if (spec.renderTargetBlends[i].allow) {
        colorBlendAttachments[i].blendEnable = VK_TRUE;
        colorBlendAttachments[i].colorBlendOp =
            getBlendOp(spec.renderTargetBlends[i].colorOp);
        colorBlendAttachments[i].srcColorBlendFactor =
            getBlendFactor(spec.renderTargetBlends[i].srcColorFactor);
        colorBlendAttachments[i].dstColorBlendFactor =
            getBlendFactor(spec.renderTargetBlends[i].dstColorFactor);
        colorBlendAttachments[i].alphaBlendOp =
            getBlendOp(spec.renderTargetBlends[i].alphaOp);
        colorBlendAttachments[i].srcAlphaBlendFactor =
            getBlendFactor(spec.renderTargetBlends[i].srcAlphaFactor);
        colorBlendAttachments[i].dstAlphaBlendFactor =
            getBlendFactor(spec.renderTargetBlends[i].dstAlphaFactor);
      } else {
        colorBlendAttachments[i].blendEnable = VK_FALSE;
      }
    }
  }

  VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
  colorBlendInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlendInfo.logicOpEnable = VK_FALSE;
  colorBlendInfo.pAttachments = colorBlendAttachments;
  colorBlendInfo.attachmentCount = spec.renderTargetCount;

  //
  // PIPELINE
  //
  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
  pipelineInfo.pViewportState = &viewportStateInfo;
  pipelineInfo.pRasterizationState = &rasterizationInfo;
  pipelineInfo.pMultisampleState = &multisampleInfo;
  pipelineInfo.pDepthStencilState = &depthStencilInfo;
  pipelineInfo.pColorBlendState = &colorBlendInfo;
  pipelineInfo.pDynamicState = &dynamicStateInfo;
  pipelineInfo.layout = pipeline->layout;
  pipelineInfo.renderPass = VK_NULL_HANDLE;
  pipelineInfo.subpass = 0;

  VkFormat *formats = nullptr;
  if (spec.renderTargetCount > 0) {
    formats = new VkFormat[spec.renderTargetCount];
    for (int i = 0; i < spec.renderTargetCount; ++i) {
      formats[i] = getVulkanFormat(spec.renderTargetFormats[i]);
    }
  }
  VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
  pipelineRenderingInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  pipelineRenderingInfo.pColorAttachmentFormats = formats;
  pipelineRenderingInfo.colorAttachmentCount = spec.renderTargetCount;
  if (spec.depthPixelFormat) {
    pipelineRenderingInfo.depthAttachmentFormat =
        getVulkanFormat(*spec.depthPixelFormat);
  }
  pipelineInfo.pNext = &pipelineRenderingInfo;

  VkPipeline vkPipeline = VK_NULL_HANDLE;
  VkResult result = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline->pipeline);

  if (spec.renderTargetCount > 0) {
    delete[] formats;
    delete[] colorBlendAttachments;
  }
  if (spec.attributeCount > 0)
    delete[] attributeDescs;
  if (spec.vertexBindingCount > 0)
    delete[] bindingDescs;

  return pipeline;
}

Pipeline createComputePipeline(LContext context,
                               const ComputePipelineSpecification &spec) {
  auto pipeline = new PipelineT;
  pipeline->layout = spec.pipelineLayout->layout;
  pipeline->isGraphics = false;

  VkPipelineShaderStageCreateInfo shaderStage{};
  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shaderStage.module = spec.computeShader->module;
  shaderStage.pName = "main";

  VkComputePipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.layout = pipeline->layout;
  pipelineInfo.stage = shaderStage;

  vkCreateComputePipelines(context->device, VK_NULL_HANDLE, 1, &pipelineInfo,
                           nullptr, &pipeline->pipeline);

  return pipeline;
}

void destroyPipeline(LContext context, Pipeline pipeline) {
  vkDestroyPipeline(context->device, pipeline->pipeline, nullptr);
  delete pipeline;
}

DescriptorSetLayout
createDescriptorSetLayout(LContext context,
                          const DescriptorSetLayoutSpecification &spec) {
  auto uniformSet = new DescriptorSetLayoutT;
  uniformSet->types = new ResourceType[spec.typeCount];
  uniformSet->typeCount = spec.typeCount;
  memcpy(uniformSet->types, spec.types, spec.typeCount * sizeof(ResourceType));
  VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
  setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  auto bindings = new VkDescriptorSetLayoutBinding[spec.typeCount]{};
  for (int i = 0; i < spec.typeCount; ++i) {
    bindings[i].binding = i;
    bindings[i].descriptorType = getDescriptorType(spec.types[i]);
    bindings[i].descriptorCount = 1;
    bindings[i].stageFlags = getShaderStageFlags(spec.visibilities[i]);
  }
  setLayoutInfo.pBindings = bindings;
  setLayoutInfo.bindingCount = spec.typeCount;
  vkCreateDescriptorSetLayout(context->device, &setLayoutInfo, nullptr,
                              &uniformSet->descriptorSetLayout);
  delete[] bindings;
  return uniformSet;
}

void destroyDescriptorSetLayout(LContext context, DescriptorSetLayout layout) {
  vkDestroyDescriptorSetLayout(context->device, layout->descriptorSetLayout,
                               nullptr);
  delete[] layout->types;
  delete layout;
}

namespace {

VkAttachmentLoadOp getLoadOperation(LoadOp loadOp) {
  switch (loadOp) {
  case LoadOp::LOAD:
    return VK_ATTACHMENT_LOAD_OP_LOAD;
  case LoadOp::CLEAR:
    return VK_ATTACHMENT_LOAD_OP_CLEAR;
  case LoadOp::DONT_CARE:
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  case LoadOp::NONE:
    return VK_ATTACHMENT_LOAD_OP_NONE;
  }

  std::println("[Vulkan] Unknown load op, defaulting to DONT_CARE");
  return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp getStoreOperation(StoreOp storeOp) {
  switch (storeOp) {
  case StoreOp::NONE:
    return VK_ATTACHMENT_STORE_OP_NONE;
  case StoreOp::DONT_CARE:
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  case StoreOp::STORE:
    return VK_ATTACHMENT_STORE_OP_STORE;
  }

  std::println("[Vulkan] Unknown store op, defaulting to DONT_CARE");
  return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

} // namespace

RenderPass createRenderPass(LContext context,
                            const RenderPassSpecification &spec) {
  auto renderPass = new RenderPassT;
  if (spec.renderTargetCount > 0) {
    renderPass->colorAttachmentInfos =
        new VkRenderingAttachmentInfo[spec.renderTargetCount]{};

    for (int i = 0; i < spec.renderTargetCount; ++i) {
      renderPass->colorAttachmentInfos[i].sType =
          VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
      renderPass->colorAttachmentInfos[i].loadOp =
          getLoadOperation(spec.renderTargets[i].loadOp);
      renderPass->colorAttachmentInfos[i].storeOp =
          getStoreOperation(spec.renderTargets[i].storeOp);
      for (int j = 0; j < 4; ++j) {
        renderPass->colorAttachmentInfos[i].clearValue.color.float32[j] =
            spec.renderTargets[i].clearColor[j];
      }
      renderPass->colorAttachmentInfos[i].imageView =
          spec.renderTargets[i].image->imageView;
      renderPass->colorAttachmentInfos[i].imageLayout =
          getImageLayout(spec.renderTargets[i].targetLayout);
      // For MSAA
      renderPass->colorAttachmentInfos[i].resolveMode = VK_RESOLVE_MODE_NONE;
      renderPass->colorAttachmentInfos[i].resolveImageView = VK_NULL_HANDLE;
      renderPass->colorAttachmentInfos[i].resolveImageLayout =
          VK_IMAGE_LAYOUT_UNDEFINED;
    }
  }
  renderPass->renderTargetCount = spec.renderTargetCount;
  renderPass->renderTargets = new Image[spec.renderTargetCount];
  for (int i = 0; i < spec.renderTargetCount; ++i) {
    renderPass->renderTargets[i] = spec.renderTargets[i].image;
  }

  renderPass->depthTarget = nullptr;
  if (spec.depthTarget) {
    renderPass->depthAttachmentInfo.sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    renderPass->depthAttachmentInfo.pNext = nullptr;
    renderPass->depthAttachmentInfo.loadOp =
        getLoadOperation(spec.depthTarget->loadOp);
    renderPass->depthAttachmentInfo.storeOp =
        getStoreOperation(spec.depthTarget->storeOp);
    renderPass->depthAttachmentInfo.clearValue.depthStencil.depth =
        spec.depthTarget->clearDepth;
    renderPass->depthAttachmentInfo.imageView =
        spec.depthTarget->image->imageView;
    renderPass->depthAttachmentInfo.imageLayout =
        getImageLayout(spec.depthTarget->targetLayout);
    // For MSAA
    renderPass->depthAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
    renderPass->depthAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
    renderPass->depthAttachmentInfo.resolveImageLayout =
        VK_IMAGE_LAYOUT_UNDEFINED;

    renderPass->depthTarget = spec.depthTarget->image;
  }

  renderPass->width = spec.width;
  renderPass->height = spec.height;

  return renderPass;
}

void destroyRenderPass(LContext context, RenderPass renderPass) {
  if (renderPass->renderTargetCount > 0)
    delete[] renderPass->colorAttachmentInfos;
  delete[] renderPass->renderTargets;
  delete renderPass;
}

Swapchain createSwapchain(LContext context,
                          const SwapchainSpecification &spec) {
  auto swapchain = new SwapchainT;

  swapchain->stabilizeFramerate = spec.stabilizeFramerate;

  VkSurfaceFormatKHR surfaceFormat =
      chooseSwapchainFormat(context->physicalDevice, context->surface);
  VkPresentModeKHR presentMode = chooseSwapchainPresentMode(
      context->physicalDevice, context->surface, spec.vSync);
  VkExtent2D extent = chooseSwapchainExtent(
      context->physicalDevice, context->surface, spec.width, spec.height);
  swapchain->width = extent.width;
  swapchain->height = extent.height;

  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physicalDevice,
                                            context->surface, &capabilities);

  uint32_t imageCount = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 &&
      imageCount > capabilities.maxImageCount) {
    imageCount = capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR swapchainInfo{};
  swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainInfo.surface = context->surface;
  swapchainInfo.minImageCount = imageCount;
  swapchainInfo.imageFormat = surfaceFormat.format;
  swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
  swapchainInfo.imageExtent = extent;
  swapchainInfo.imageArrayLayers = 1;
  swapchainInfo.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainInfo.queueFamilyIndexCount = 0;
  swapchainInfo.pQueueFamilyIndices = nullptr;
  swapchainInfo.preTransform = capabilities.currentTransform;
  swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainInfo.presentMode = presentMode;
  swapchainInfo.clipped = VK_TRUE;
  swapchainInfo.oldSwapchain =
      spec.retiredSwapchain ? spec.retiredSwapchain->swapchain : VK_NULL_HANDLE;

  swapchain->format = surfaceFormat.format;

  auto result = vkCreateSwapchainKHR(context->device, &swapchainInfo, nullptr,
                                     &swapchain->swapchain);

  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to create swapchain: {}", (int)result);
    delete swapchain;
    return nullptr;
  }

  result = vkGetSwapchainImagesKHR(context->device, swapchain->swapchain,
                                   &swapchain->imageCount, nullptr);

  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to get swapchain images: {}", (int)result);
    vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
    delete swapchain;
    return nullptr;
  }

  swapchain->images = new VkImage[swapchain->imageCount];
  vkGetSwapchainImagesKHR(context->device, swapchain->swapchain,
                          &swapchain->imageCount, swapchain->images);

  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to get swapchain images: {}", (int)result);
    delete[] swapchain->images;
    vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
    delete swapchain;
    return nullptr;
  }

  swapchain->imageViews = new VkImageView[swapchain->imageCount];

  for (uint32_t i = 0; i < swapchain->imageCount; i++) {
    VkImageViewCreateInfo imageViewInfo{};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.image = swapchain->images[i];
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.format = surfaceFormat.format;
    imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewInfo.subresourceRange.baseMipLevel = 0;
    imageViewInfo.subresourceRange.levelCount = 1;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount = 1;

    result = vkCreateImageView(context->device, &imageViewInfo, nullptr,
                               &swapchain->imageViews[i]);
    if (result != VK_SUCCESS) {
      std::println(stderr, "[Vulkan] Failed to create image view {}: {}", i,
                   (int)result);
      for (uint32_t j = 0; j < i; j++) {
        vkDestroyImageView(context->device, swapchain->imageViews[j], nullptr);
      }
      delete[] swapchain->images;
      delete[] swapchain->imageViews;
      vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
      delete swapchain;
      return nullptr;
    }
  }

  VkCommandPoolCreateInfo commandPoolInfo{};
  commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolInfo.queueFamilyIndex = context->graphicsQueueFamilyIndex;
  commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  result = vkCreateCommandPool(context->device, &commandPoolInfo, nullptr,
                               &swapchain->commandPool);

  if (result != VK_SUCCESS) {
    std::println(stderr, "[Vulkan] Failed to create swapchain command pool: {}",
                 (int)result);
    for (uint32_t i = 0; i < swapchain->imageCount; i++) {
      vkDestroyImageView(context->device, swapchain->imageViews[i], nullptr);
    }
    delete[] swapchain->images;
    delete[] swapchain->imageViews;
    vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
    delete swapchain;
    return nullptr;
  }

  VkCommandBufferAllocateInfo allocateInfo{};
  allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocateInfo.commandPool = swapchain->commandPool;
  allocateInfo.commandBufferCount = 1;
  result = vkAllocateCommandBuffers(context->device, &allocateInfo,
                                    &swapchain->commandBuffer);

  if (result != VK_SUCCESS) {
    std::println(stderr,
                 "[Vulkan] Failed to allocate swapchain command buffer: {}",
                 (int)result);
    vkDestroyCommandPool(context->device, swapchain->commandPool, nullptr);
    for (uint32_t i = 0; i < swapchain->imageCount; i++) {
      vkDestroyImageView(context->device, swapchain->imageViews[i], nullptr);
    }
    delete[] swapchain->images;
    delete[] swapchain->imageViews;
    vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
    delete swapchain;
    return nullptr;
  }

  swapchain->presentQueue = context->graphicsQueue;

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  result = vkCreateSemaphore(context->device, &semaphoreInfo, nullptr,
                             &swapchain->blitSemaphore);

  if (result != VK_SUCCESS) {
    std::println(stderr,
                 "[Vulkan] Failed to create swapchain blit semaphore: {}",
                 (int)result);
    vkDestroyCommandPool(context->device, swapchain->commandPool, nullptr);
    for (uint32_t i = 0; i < swapchain->imageCount; i++) {
      vkDestroyImageView(context->device, swapchain->imageViews[i], nullptr);
    }
    delete[] swapchain->images;
    delete[] swapchain->imageViews;
    vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
    delete swapchain;
    return nullptr;
  }

  vkCreateSemaphore(context->device, &semaphoreInfo, nullptr,
                    &swapchain->acquireSemaphore);

  if (result != VK_SUCCESS) {
    std::println(stderr,
                 "[Vulkan] Failed to create swapchain acquire semaphore: {}",
                 (int)result);
    vkDestroySemaphore(context->device, swapchain->blitSemaphore, nullptr);
    vkDestroyCommandPool(context->device, swapchain->commandPool, nullptr);
    for (uint32_t i = 0; i < swapchain->imageCount; i++) {
      vkDestroyImageView(context->device, swapchain->imageViews[i], nullptr);
    }
    delete[] swapchain->images;
    delete[] swapchain->imageViews;
    vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
    delete swapchain;
    return nullptr;
  }

  return swapchain;
}

void present(LContext context, Swapchain swapchain, Image image,
             ImageMetadata meta, ImageMetadata *outMeta,
             SampleFilter blitFilter) {
  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(
      context->device, swapchain->swapchain, UINT64_MAX,
      swapchain->acquireSemaphore, VK_NULL_HANDLE, &imageIndex);

  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Swapchain could not acquire image");
    return;
  }

  VkImage swapImage = swapchain->images[imageIndex];

  vkResetCommandPool(context->device, swapchain->commandPool, 0);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(swapchain->commandBuffer, &beginInfo);

  VkImageMemoryBarrier2 barriers[2];
  ImageMetadata imageMeta = meta;
  if (imageMeta.ownership == QueueOwnership::NONE) {
    // TODO: More validation
    imageMeta.ownership = QueueOwnership::GRAPHICS;
  }
  ImageMetadata transferSrcMeta;
  transferSrcMeta.layout = ImageLayout::TRANSFER_SRC;
  transferSrcMeta.ownership = QueueOwnership::GRAPHICS;
  transferSrcMeta.stage = PipelineStage::TRANSFER;
  transferSrcMeta.access = ResourceAccess::TRANSFER_READ;
  createImageBarrier(context, swapchain->commandBuffer, image, imageMeta,
                     transferSrcMeta, barriers[0]);

  if (outMeta) {
    *outMeta = transferSrcMeta;
  }

  ImageMetadata sourceMeta;
  sourceMeta.layout = ImageLayout::UNDEFINED;
  sourceMeta.ownership = QueueOwnership::GRAPHICS;
  sourceMeta.stage = PipelineStage::TOP_OF_PIPE;
  sourceMeta.access = ResourceAccess::NONE;

  ImageMetadata transferDstMeta;
  transferDstMeta.layout = ImageLayout::TRANSFER_DST;
  transferDstMeta.ownership = QueueOwnership::GRAPHICS;
  transferDstMeta.stage = PipelineStage::TRANSFER;
  transferDstMeta.access = ResourceAccess::TRANSFER_WRITE;
  ImageT swapImageWrapper{};
  swapImageWrapper.image = swapImage;
  swapImageWrapper.isDepth = false;
  createImageBarrier(context, swapchain->commandBuffer, &swapImageWrapper,
                     sourceMeta, transferDstMeta, barriers[1]);

  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependencyInfo.pImageMemoryBarriers = barriers;
  dependencyInfo.imageMemoryBarrierCount = 2;
  vkCmdPipelineBarrier2(swapchain->commandBuffer, &dependencyInfo);

  // TODO: MSAA mismatch also requires blit
  if (image->width == swapchain->width && image->height == swapchain->height &&
      image->format == swapchain->format) {
    copyImage(swapchain->commandBuffer, image->image, swapImage,
              swapchain->width, swapchain->height);
  } else {
    blitImage(swapchain->commandBuffer, image->image, swapImage, image->width,
              image->height, swapchain->width, swapchain->height,
              getVulkanFilter(blitFilter));
  }

  ImageMetadata presentMeta;
  presentMeta.layout = ImageLayout::PRESENT;
  presentMeta.ownership = QueueOwnership::GRAPHICS;
  presentMeta.stage = PipelineStage::BOTTOM_OF_PIPE;
  presentMeta.access = ResourceAccess::NONE;
  createImageBarrier(context, swapchain->commandBuffer, &swapImageWrapper,
                     transferDstMeta, presentMeta, barriers[0]);
  dependencyInfo.imageMemoryBarrierCount = 1;
  vkCmdPipelineBarrier2(swapchain->commandBuffer, &dependencyInfo);

  vkEndCommandBuffer(swapchain->commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.pCommandBuffers = &swapchain->commandBuffer;
  submitInfo.commandBufferCount = 1;
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
  submitInfo.pWaitSemaphores = &swapchain->acquireSemaphore;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &swapchain->blitSemaphore;
  submitInfo.signalSemaphoreCount = 1;
  vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain->swapchain;
  presentInfo.pImageIndices = &imageIndex;
  presentInfo.pWaitSemaphores = &swapchain->blitSemaphore;
  presentInfo.waitSemaphoreCount = 1;

  // TODO: This should be capped at monitor refresh rate if VSync is on.
  if (swapchain->stabilizeFramerate != 0) {
    const uint64_t target = 1000000000 / swapchain->stabilizeFramerate;
    uint64_t frameTime =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto delta = frameTime - swapchain->lastFrameTime;
    // Sleep if we have time until the end of the frame. This makes sure that
    // all frames are equally long
    if (delta < target) {
      auto delay = std::chrono::nanoseconds(target - delta);
      std::this_thread::sleep_for(delay);
    }

    // After potentially sleeping, remember the time.
    swapchain->lastFrameTime =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
  }
  vkQueuePresentKHR(swapchain->presentQueue, &presentInfo);

  vkQueueWaitIdle(swapchain->presentQueue);
}

void destroySwapchain(LContext context, Swapchain swapchain) {
  vkDestroySemaphore(context->device, swapchain->acquireSemaphore, nullptr);
  vkDestroySemaphore(context->device, swapchain->blitSemaphore, nullptr);
  vkDestroyCommandPool(context->device, swapchain->commandPool, nullptr);
  for (uint32_t i = 0; i < swapchain->imageCount; i++) {
    vkDestroyImageView(context->device, swapchain->imageViews[i], nullptr);
  }
  delete[] swapchain->imageViews;
  delete[] swapchain->images;
  vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
  delete swapchain;
}

DescriptorPool createDescriptorPool(LContext context,
                                    const DescriptorPoolSpecification &spec) {
  auto descriptorPool = new DescriptorPoolT;

  VkDescriptorPoolSize poolSizes[4];
  uint32_t poolCount = 0;

  if (spec.uniformBufferCount > 0) {
    poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[poolCount].descriptorCount = spec.uniformBufferCount;
    ++poolCount;
  }

  if (spec.combinedSamplerCount > 0) {
    poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[poolCount].descriptorCount = spec.combinedSamplerCount;
    ++poolCount;
  }

  if (spec.sampledImageCount > 0) {
    poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[poolCount].descriptorCount = spec.sampledImageCount;
    ++poolCount;
  }

  if (spec.storageImageCount > 0) {
    poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[poolCount].descriptorCount = spec.storageImageCount;
    ++poolCount;
  }

  if (poolCount == 0 || spec.maxSets == 0) {
    return nullptr;
  }

  VkDescriptorPoolCreateInfo descriptorPoolInfo{};
  descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolInfo.pPoolSizes = poolSizes;
  descriptorPoolInfo.poolSizeCount = poolCount;
  descriptorPoolInfo.maxSets = spec.maxSets;

  vkCreateDescriptorPool(context->device, &descriptorPoolInfo, nullptr,
                         &descriptorPool->descriptorPool);

  return descriptorPool;
}

void destroyDescriptorPool(LContext context, DescriptorPool descriptorPool) {
  vkDestroyDescriptorPool(context->device, descriptorPool->descriptorPool,
                          nullptr);
  delete descriptorPool;
}

DescriptorSet createDescriptorSet(LContext context,
                                  DescriptorPool descriptorPool,
                                  DescriptorSetLayout descriptorSetLayout) {
  auto descriptorSet = new DescriptorSetT;
  descriptorSet->pool = descriptorPool;

  VkDescriptorSetAllocateInfo allocateInfo{};
  allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocateInfo.descriptorPool = descriptorPool->descriptorPool;
  allocateInfo.pSetLayouts = &descriptorSetLayout->descriptorSetLayout;
  allocateInfo.descriptorSetCount = 1;
  auto res = vkAllocateDescriptorSets(context->device, &allocateInfo,
                                      &descriptorSet->descriptorSet);
  if (res != VK_SUCCESS) {
    std::println("[Vulkan] Failed to allocate descriptor set: {}", (int)res);
  }

  return descriptorSet;
}

void updateDescriptorSet(LContext context,
                         const DescriptorSetUpdateSpecification &spec) {
  if (spec.specs) {
    uint32_t specsRemaining = spec.specCount;
    const uint32_t maxWrites = 64;
    const uint32_t loopCount = (spec.specCount + maxWrites - 1) / maxWrites;
    const uint32_t finalRemainder = spec.specCount % maxWrites;
    for (uint32_t i = 0; i < loopCount; ++i) {
      VkWriteDescriptorSet writes[maxWrites];
      VkDescriptorBufferInfo bufferInfo[maxWrites];
      VkDescriptorImageInfo imageInfo[maxWrites];
      uint32_t baseIndex = i * maxWrites;
      uint32_t writeCount = (i == loopCount - 1 && finalRemainder != 0)
                                ? (spec.specCount % maxWrites)
                                : maxWrites;

      for (int j = 0; j < writeCount; ++j) {
        int index = baseIndex + j;
        auto type = spec.specs[index].type;
        writes[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[j].descriptorType = getDescriptorType(type);
        // TODO: Bunch these up if possible
        writes[j].descriptorCount = 1;
        writes[j].dstBinding = index;
        writes[j].dstSet = spec.descriptorSet->descriptorSet;
        writes[j].dstArrayElement = 0;
        writes[j].pBufferInfo = nullptr;
        writes[j].pImageInfo = nullptr;
        writes[j].pTexelBufferView = nullptr;
        writes[j].pNext = nullptr;
        if (type == ResourceType::UNIFORM_BUFFER) {
          bufferInfo[j].buffer = spec.specs[index].buffer->buffer;
          bufferInfo[j].offset = spec.specs[index].offset;
          bufferInfo[j].range = spec.specs[index].buffer->size;
          writes[j].pBufferInfo = &bufferInfo[j];
        } else if (type == ResourceType::COMBINED_SAMPLER) {
          imageInfo[j].imageView = spec.specs[index].image->imageView;
          imageInfo[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
          imageInfo[j].sampler = spec.specs[index].sampler->sampler;
          writes[j].pImageInfo = &imageInfo[j];
        } else if (type == ResourceType::SAMPLED_IMAGE) {
          imageInfo[j].imageView = spec.specs[index].image->imageView;
          imageInfo[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
          imageInfo[j].sampler = VK_NULL_HANDLE;
          writes[j].pImageInfo = &imageInfo[j];
        } else if (type == ResourceType::STORAGE_IMAGE) {
          imageInfo[j].imageView = spec.specs[index].image->imageView;
          imageInfo[j].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
          imageInfo[j].sampler = VK_NULL_HANDLE;
          writes[j].pImageInfo = &imageInfo[j];
        } else if (type == ResourceType::SAMPLER) {
          imageInfo[j].imageView = VK_NULL_HANDLE;
          imageInfo[j].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
          imageInfo[j].sampler = spec.specs[index].sampler->sampler;
          writes[j].pImageInfo = &imageInfo[j];
        } else {
          std::println(
              "[Vulkan] Unsupported type for descriptor set update: {}",
              (int)type);
        }
      }

      vkUpdateDescriptorSets(context->device, writeCount, writes, 0, nullptr);
    }
  }
}

void destroyDescriptorSet(LContext context, DescriptorSet descriptorSet) {
  delete descriptorSet;
}

DrawProcessor createDrawProcessor(LContext context) {
  auto drawProcessor = new DrawProcessorT;

  VkCommandPoolCreateInfo commandPoolInfo{};
  commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolInfo.queueFamilyIndex = context->graphicsQueueFamilyIndex;
  vkCreateCommandPool(context->device, &commandPoolInfo, nullptr,
                      &drawProcessor->commandPool);

  VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
  commandBufferAllocateInfo.sType =
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandBufferCount = 1;
  commandBufferAllocateInfo.commandPool = drawProcessor->commandPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkAllocateCommandBuffers(context->device, &commandBufferAllocateInfo,
                           &drawProcessor->primaryCommandBuffer);

  drawProcessor->secondaryCommandBuffers = nullptr;
  drawProcessor->secondaryCommandBufferCount = 0;

  return drawProcessor;
}

void setDrawProcessorSecondaries(LContext context, DrawProcessor drawProcessor,
                                 uint32_t secondaryCount) {
  if (drawProcessor->secondaryCommandBufferCount == secondaryCount)
    return;

  if (drawProcessor->secondaryCommandBufferCount > 0) {
    vkFreeCommandBuffers(context->device, drawProcessor->commandPool,
                         drawProcessor->secondaryCommandBufferCount,
                         drawProcessor->secondaryCommandBuffers);
    delete[] drawProcessor->secondaryCommandBuffers;
    delete[] drawProcessor->isRecorded;
    drawProcessor->secondaryCommandBuffers = nullptr;
    drawProcessor->isRecorded = nullptr;
  }

  if (secondaryCount > 0) {
    drawProcessor->secondaryCommandBuffers =
        new VkCommandBuffer[secondaryCount];
    drawProcessor->isRecorded = new bool[secondaryCount]{};
    drawProcessor->secondaryCommandBufferCount = secondaryCount;

    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = drawProcessor->commandPool;
    allocateInfo.commandBufferCount = secondaryCount;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    vkAllocateCommandBuffers(context->device, &allocateInfo,
                             drawProcessor->secondaryCommandBuffers);
  }
}

void resetDrawProcessor(LContext context, DrawProcessor drawProcessor) {
  for (int i = 0; drawProcessor->secondaryCommandBufferCount; ++i) {
    drawProcessor->isRecorded[i] = false;
  }
  vkResetCommandPool(context->device, drawProcessor->commandPool, 0);
}

namespace {

struct ProcessingState {
  Pipeline boundPipeline = nullptr;
  // TODO: Make this a public enum that can be returned
  bool error = false;
};

void processBeginRenderPass(LContext context, VkCommandBuffer commandBuffer,
                            const DrawCommand &drawCommand, uint32_t target,
                            uint32_t count, ProcessingState &state) {
  if (count != 1) {
    std::println(
        "[Vulkan] Beginning multiple render passes at once is not allowed");
    state.error = true;
    return;
  }
  RenderPass renderPass = drawCommand.renderPasses[target];

  VkRenderingInfo renderingInfo{};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderingInfo.pColorAttachments = renderPass->colorAttachmentInfos;
  renderingInfo.colorAttachmentCount = renderPass->renderTargetCount;
  renderingInfo.pDepthAttachment =
      renderPass->depthTarget ? &renderPass->depthAttachmentInfo : nullptr;
  renderingInfo.layerCount = 1;
  renderingInfo.renderArea = {{0, 0}, {renderPass->width, renderPass->height}};
  renderingInfo.viewMask = 0;
  vkCmdBeginRendering(commandBuffer, &renderingInfo);
}

void processEndRenderPass(LContext context, VkCommandBuffer commandBuffer,
                          const DrawCommand &drawCommand, uint32_t target,
                          uint32_t count, ProcessingState &state) {
  if (count != 1) {
    std::println(
        "[Vulkan] Ending multiple render passes at once is not allowed");
    state.error = true;
    return;
  }
  vkCmdEndRendering(commandBuffer);
}

void processImageBarrier(LContext context, VkCommandBuffer commandBuffer,
                         const DrawCommand &drawCommand, uint32_t target,
                         uint32_t count, ProcessingState &state) {
  const uint32_t maxBarriersAtOnce = 16;
  uint32_t remainingCount = count;
  for (uint32_t k = 0; k < (count + maxBarriersAtOnce - 1) / maxBarriersAtOnce;
       ++k) {
    VkImageMemoryBarrier2 barriers[maxBarriersAtOnce];

    uint32_t actualCount = std::min(remainingCount, maxBarriersAtOnce);
    for (int i = 0; i < actualCount; ++i) {
      const ImageBarrier &imageBarrier =
          drawCommand.imageBarriers[k * maxBarriersAtOnce + target + i];

      createImageBarrier(context, commandBuffer, imageBarrier.image,
                         imageBarrier.fromMeta, imageBarrier.toMeta,
                         barriers[i]);
    }

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pImageMemoryBarriers = barriers;
    dependencyInfo.imageMemoryBarrierCount = actualCount;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

    remainingCount -= actualCount;
  }
}

void processBufferBarrier(LContext context, VkCommandBuffer commandBuffer,
                          const DrawCommand &drawCommand, uint32_t target,
                          uint32_t count, ProcessingState &state) {
  const uint32_t maxBarriersAtOnce = 16;
  uint32_t remainingCount = count;
  for (uint32_t k = 0; k < (count + maxBarriersAtOnce - 1) / maxBarriersAtOnce;
       ++k) {
    VkBufferMemoryBarrier2 barriers[maxBarriersAtOnce];

    uint32_t actualCount = std::min(remainingCount, maxBarriersAtOnce);
    for (int i = 0; i < actualCount; ++i) {
      const BufferBarrier &bufferBarrier =
          drawCommand.bufferBarriers[k * maxBarriersAtOnce + target + i];

      createBufferBarrier(context, commandBuffer, bufferBarrier.buffer,
                          bufferBarrier.fromMeta, bufferBarrier.toMeta,
                          barriers[i]);
    }

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pBufferMemoryBarriers = barriers;
    dependencyInfo.bufferMemoryBarrierCount = actualCount;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

    remainingCount -= actualCount;
  }
}

void processBindPipeline(LContext context, VkCommandBuffer commandBuffer,
                         const DrawCommand &drawCommand, uint32_t target,
                         uint32_t count, ProcessingState &state) {
  if (count != 1) {
    // TODO: This could be allowed - to bind one graphics and one compute
    // pipeline at once, but perhaps it is better to be explicit
    std::println(
        "[Vulkan] Binding multiple consecutive pipelines is not allowed");
    state.error = true;
    return;
  }
  Pipeline pipeline = drawCommand.pipelines[target];

  VkPipelineBindPoint bindPoint;
  if (pipeline->isGraphics)
    bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  else
    bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

  vkCmdBindPipeline(commandBuffer, bindPoint, pipeline->pipeline);

  state.boundPipeline = pipeline;
}

void processBindVertexBuffer(LContext context, VkCommandBuffer commandBuffer,
                             const DrawCommand &drawCommand, uint32_t target,
                             uint32_t count, ProcessingState &state) {
  if (count > 16) {
    // TODO: Could quietly process this as multiple vkCmd calls
    std::println(
        "[Vulkan] A maximum of 16 vertex buffers can be bound at once");
    state.error = true;
    return;
  }
  uint32_t firstBinding = drawCommand.vertexBindings[target].binding;
  if (firstBinding == UINT32_MAX)
    firstBinding = 0;

  VkBuffer buffers[16];
  VkDeviceSize offsets[16];
  for (int i = 0; i < count; ++i) {
    buffers[i] = drawCommand.vertexBindings[target + i].buffer->buffer;
    offsets[i] = drawCommand.vertexBindings[target + i].offset;
    uint32_t binding = drawCommand.vertexBindings[target + i].binding;
    if (firstBinding + i != binding && binding != UINT32_MAX) {
      // TODO: Could quietly process this as multiple vkCmd calls
      std::println("[Vulkan] Bulked vertex bindings must have consecutive bind "
                   "points or be UINT32_MAX");
      state.error = true;
      return;
    }
  }

  vkCmdBindVertexBuffers(commandBuffer, firstBinding, count, buffers, offsets);
}

void processBindIndexBuffer(LContext context, VkCommandBuffer commandBuffer,
                            const DrawCommand &drawCommand, uint32_t target,
                            uint32_t count, ProcessingState &state) {
  if (count != 1) {
    std::println(
        "[Vulkan] Binding multiple consecutive index buffers is not allowed");
    state.error = true;
    return;
  }
  const IndexBinding &indexBinding = drawCommand.indexBindings[target];

  VkDeviceSize offset = indexBinding.offset;
  vkCmdBindIndexBuffer(commandBuffer, indexBinding.buffer->buffer, offset,
                       VK_INDEX_TYPE_UINT32);
}

void processBindDescriptorSets(LContext context, VkCommandBuffer commandBuffer,
                               const DrawCommand &drawCommand, uint32_t target,
                               uint32_t count, ProcessingState &state) {
  if (count > 4) {
    std::println("[Vulkan] A maximum of 4 descriptors can be bound at once");
    state.error = true;
    return;
  }

  Pipeline pipeline = state.boundPipeline;
  if (pipeline == VK_NULL_HANDLE) {
    std::println("[Vulkan] A pipeline must be bound to process descriptor "
                 "set binding");
    state.error = true;
    return;
  }
  VkPipelineBindPoint bindPoint;
  if (pipeline->isGraphics)
    bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  else
    bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

  uint32_t firstBinding = drawCommand.descriptorSetBindings[target].binding;
  if (firstBinding == UINT32_MAX)
    firstBinding = 0;

  // TODO: Could add dynamic offsets for flexibility
  VkDescriptorSet descriptorSets[4];
  for (int i = 0; i < count; ++i) {
    descriptorSets[i] =
        drawCommand.descriptorSetBindings[target + i].set->descriptorSet;
    uint32_t binding = drawCommand.descriptorSetBindings[target + i].binding;
    if (firstBinding + i != binding && binding != UINT32_MAX) {
      // TODO: Could quietly process this as multiple vkCmd calls
      std::println("[Vulkan] Bulked descriptor set bindings must have "
                   "consecutive bind points or be UINT32_MAX");
      state.error = true;
      return;
    }
  }

  vkCmdBindDescriptorSets(commandBuffer, bindPoint, pipeline->layout,
                          firstBinding, count, descriptorSets, 0, nullptr);
}

void processPushConstant(LContext context, VkCommandBuffer commandBuffer,
                         const DrawCommand &drawCommand, uint32_t target,
                         uint32_t count, ProcessingState &state) {
  for (int i = 0; i < count; ++i) {
    const PushConstantUpload &upload = drawCommand.pushConstants[target + i];
    vkCmdPushConstants(commandBuffer, state.boundPipeline->layout,
                       getShaderStageFlags(upload.visibility), upload.offset,
                       upload.size, upload.bytes);
  }
}

void processDraw(LContext context, VkCommandBuffer commandBuffer,
                 const DrawCommand &drawCommand, uint32_t target,
                 uint32_t count, ProcessingState &state) {
  for (int i = 0; i < count; ++i) {
    const DrawCall &drawCallData = drawCommand.drawCalls[target];

    // TODO: This could be split into separate enum op values and separate
    // process functions to avoid this conditional jump
    if (drawCallData.indexed) {
      vkCmdDrawIndexed(commandBuffer, drawCallData.elementCount,
                       drawCallData.instanceCount, drawCallData.firstElement,
                       drawCallData.vertexOffset, drawCallData.firstInstance);
    } else {
      vkCmdDraw(commandBuffer, drawCallData.elementCount,
                drawCallData.instanceCount, drawCallData.firstElement,
                drawCallData.firstInstance);
    }
  }
}

void processDispatch(LContext context, VkCommandBuffer commandBuffer,
                     const DrawCommand &drawCommand, uint32_t target,
                     uint32_t count, ProcessingState &state) {
  for (int i = 0; i < count; ++i) {
    const DispatchCall &dispatchCall = drawCommand.dispatchCalls[target];
    vkCmdDispatch(commandBuffer, dispatchCall.groupCountX,
                  dispatchCall.groupCountY, dispatchCall.groupCountZ);
  }
}

void processBlit(LContext context, VkCommandBuffer commandBuffer,
                 const DrawCommand &drawCommand, uint32_t target,
                 uint32_t count, ProcessingState &state) {
  for (int i = 0; i < count; ++i) {
    const BlitCall &blitCall = drawCommand.blitCalls[target + i];
    blitImage(commandBuffer, blitCall.from->image, blitCall.to->image,
              blitCall.from->width, blitCall.from->height, blitCall.to->width,
              blitCall.to->height, getVulkanFilter(blitCall.filter));
  }
}

void dispatchProcessing(LContext context, VkCommandBuffer commandBuffer,
                        const DrawCommand &drawCommand, DrawOperationType type,
                        uint32_t target, uint32_t count,
                        ProcessingState &state) {
  using Sig = void (*)(LContext, VkCommandBuffer, const DrawCommand &, uint32_t,
                       uint32_t, ProcessingState &);
  const Sig processors[] = {
      processBeginRenderPass, processEndRenderPass,
      processImageBarrier,    processBufferBarrier,
      processBindPipeline,    processBindDescriptorSets,
      processPushConstant,    processBindVertexBuffer,
      processBindIndexBuffer, processDraw,
      processDispatch,        processBlit,
  };

  processors[(uint32_t)type](context, commandBuffer, drawCommand, target, count,
                             state);
}

void recordDrawCommand(LContext context, VkCommandBuffer commandBuffer,
                       const DrawCommand &drawCommand, uint32_t operationCount,
                       uint32_t graphicsQueueFamilyIndex) {
  ProcessingState state{};

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  for (int i = 0; i < operationCount; ++i) {
    DrawOperationType type = drawCommand.operations[i].type;
    uint32_t target = drawCommand.operations[i].target;
    uint32_t count = drawCommand.operations[i].count;
    dispatchProcessing(context, commandBuffer, drawCommand, type, target, count,
                       state);
    if (state.error) {
      std::println(
          "[Vulkan] Stopping the draw command processing prematurely...");
      break;
    }
  }

  vkEndCommandBuffer(commandBuffer);
}

} // namespace

void processDrawSecondary(LContext context, DrawProcessor drawProcessor,
                          const DrawCommand &drawCommand,
                          uint32_t operationCount, uint32_t secondaryIndex) {
  VkCommandBuffer commandBuffer =
      drawProcessor->secondaryCommandBuffers[secondaryIndex];

  drawProcessor->isRecorded[secondaryIndex] = true;

  recordDrawCommand(context, commandBuffer, drawCommand, operationCount,
                    context->graphicsQueueFamilyIndex);
}

void processDraw(LContext context, DrawProcessor drawProcessor,
                 const DrawCommand &drawCommand, uint32_t operationCount) {
  VkCommandBuffer commandBuffer = drawProcessor->primaryCommandBuffer;

  recordDrawCommand(context, commandBuffer, drawCommand, operationCount,
                    context->graphicsQueueFamilyIndex);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

  vkQueueWaitIdle(context->graphicsQueue);
}

void submitSecondaries(LContext context, DrawProcessor drawProcessor) {
  VkCommandBuffer commandBuffer = drawProcessor->primaryCommandBuffer;

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  uint32_t executeCount = 0;
  for (int i = 0; i < drawProcessor->secondaryCommandBufferCount; ++i) {
    if (i != executeCount && drawProcessor->isRecorded[i]) {
      std::swap(drawProcessor->isRecorded[executeCount],
                drawProcessor->isRecorded[i]);
      std::swap(drawProcessor->secondaryCommandBuffers[executeCount],
                drawProcessor->secondaryCommandBuffers[i]);
    }
    if (drawProcessor->isRecorded[i]) {
      ++executeCount;
    }
  }

  vkCmdExecuteCommands(commandBuffer,
                       drawProcessor->secondaryCommandBufferCount,
                       drawProcessor->secondaryCommandBuffers);

  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

  vkQueueWaitIdle(context->graphicsQueue);
}

void destroyDrawProcessor(LContext context, DrawProcessor drawProcessor) {
  vkDestroyCommandPool(context->device, drawProcessor->commandPool, nullptr);
  delete drawProcessor;
}
