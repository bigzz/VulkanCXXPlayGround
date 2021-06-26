#pragma once

#include "cfx_device.hpp"

#include<vulkan/vulkan.h>
#include<string>
#include<vector>
#include <memory>


namespace cfx{
    class CFXSwapChain{
        public:
  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

  CFXSwapChain(CFXDevice &deviceRef, VkExtent2D windowExtent);
  CFXSwapChain(CFXDevice &deviceRef, VkExtent2D windowExtent,std::shared_ptr<CFXSwapChain> previous);
  ~CFXSwapChain();

  CFXSwapChain(const CFXSwapChain &) = delete;
  void operator=(const CFXSwapChain &) = delete;

  VkFramebuffer getFrameBuffer(int index) { return swapChainFramebuffers[index]; }
  VkRenderPass getRenderPass() { return renderPass; }
  VkImageView getImageView(int index) { return swapChainImageViews[index]; }
  size_t imageCount() { return swapChainImages.size(); }
  VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }
  VkExtent2D getSwapChainExtent() { return swapChainExtent; }
  uint32_t width() { return swapChainExtent.width; }
  uint32_t height() { return swapChainExtent.height; }

  float extentAspectRatio() {
    return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
  }
  VkFormat findDepthFormat();

  VkResult acquireNextImage(uint32_t *imageIndex);
  VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex,uint32_t deviceIndex);
  bool compareSwapFormats(const CFXSwapChain& cfxSwapChain) const {
      return cfxSwapChain.swapChainDepthFormat == swapChainDepthFormat && cfxSwapChain.swapChainImageFormat == swapChainImageFormat;
  }

 private:
  void createSwapChain();
  void init();
  void createImageViews();
  void createDepthResources();
  void createRenderPass();
  void createFramebuffers();
  void createSyncObjects();

  // Helper functions
  VkSurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &availableFormats);
  VkPresentModeKHR chooseSwapPresentMode(
      const std::vector<VkPresentModeKHR> &availablePresentModes);
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

  VkFormat swapChainImageFormat;
  VkFormat swapChainDepthFormat;
  VkExtent2D swapChainExtent;

  std::vector<VkFramebuffer> swapChainFramebuffers;
  VkRenderPass renderPass;

  std::vector<VkImage> depthImages;
  std::vector<VkDeviceMemory> depthImageMemorys;
  std::vector<VkImageView> depthImageViews;
  std::vector<VkImage> swapChainImages;
  std::vector<VkImageView> swapChainImageViews;

  CFXDevice &device;
  VkExtent2D windowExtent;

  VkSwapchainKHR swapChain;
  std::shared_ptr<CFXSwapChain> oldSwapChain;


  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;
  std::vector<VkFence> imagesInFlight;
  size_t currentFrame = 0;
  std::vector<uint32_t> deviceMasks = {1,2};
    };
}