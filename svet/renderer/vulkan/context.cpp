#include "context.h"
#include "types.h"

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <cstring>
#include <print>

namespace svet::renderer {

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

  const char *enabledLayers[] = {"VK_LAYER_KHRONOS_validation"};
  if (spec.allowValidation) {
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

  VkPhysicalDeviceProperties deviceProperties;
  vkGetPhysicalDeviceProperties(context->physicalDevice, &deviceProperties);
  context->atomicFlushSize = deviceProperties.limits.nonCoherentAtomSize;

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
  // Since Vulkan 1.0, only instance-level layers are supported
  deviceInfo.ppEnabledLayerNames = nullptr;
  deviceInfo.enabledLayerCount = 0;

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

} // namespace svet::renderer
