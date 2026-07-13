#include "render-pass.h"
#include "helpers.h"
#include "types.h"

#include <vulkan/vulkan.h>

#include <print>

namespace svet::renderer {

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

} // namespace svet::renderer
