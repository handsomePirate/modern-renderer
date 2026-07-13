#include "shader.h"
#include "types.h"

#include <vulkan/vulkan.h>

namespace svet::renderer {

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

} // namespace svet::renderer
