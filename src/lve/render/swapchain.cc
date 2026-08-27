#include "render.hh"

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <limits>
#include <ranges>
#include <stdexcept>

#include <SDL3/SDL.h>

auto Renderer::drawable_extent_() const -> std::optional<vk::Extent2D>
{
  auto const flags = SDL_GetWindowFlags(&window_);
  if (flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) return std::nullopt;

  auto width = int{};
  auto height = int{};
  if (!SDL_GetWindowSizeInPixels(&window_, &width, &height))
    throw std::runtime_error{
      std::format("SDL drawable size query failed: {}", SDL_GetError())
    };
  if (width <= 0 || height <= 0) return std::nullopt;

  return vk::Extent2D{
    .width = static_cast<std::uint32_t>(width),
    .height = static_cast<std::uint32_t>(height),
  };
}

auto Renderer::create_swapchain_(
  vk::Extent2D drawable_extent, vk::SwapchainKHR old_swapchain
) -> SwapchainResources
{
  auto resources = SwapchainResources{};
  auto const capabilities =
    context_.physical_device.getSurfaceCapabilitiesKHR(*context_.surface);
  auto const formats =
    context_.physical_device.getSurfaceFormatsKHR(*context_.surface);
  auto const present_modes =
    context_.physical_device.getSurfacePresentModesKHR(*context_.surface);
  if (formats.empty() || present_modes.empty())
    throw std::runtime_error{"Vulkan surface has no usable swapchain settings"};
  if (!(capabilities.supportedUsageFlags &
        vk::ImageUsageFlagBits::eColorAttachment))
    throw std::runtime_error{
      "Vulkan surface cannot be used as a color attachment"
    };

  auto const surface_format = choose_surface_format_(formats);
  resources.format = surface_format.format;
  resources.color_space = surface_format.colorSpace;
  resources.extent = choose_extent_(capabilities, drawable_extent);

  auto image_count = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0)
    image_count = std::min(image_count, capabilities.maxImageCount);

  auto const queue_families =
    std::array{context_.graphics_queue_family, context_.present_queue_family};
  auto const separate_queues =
    context_.graphics_queue_family != context_.present_queue_family;
  auto const create_info = vk::SwapchainCreateInfoKHR{
    .surface = *context_.surface,
    .minImageCount = image_count,
    .imageFormat = resources.format,
    .imageColorSpace = resources.color_space,
    .imageExtent = resources.extent,
    .imageArrayLayers = 1,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
    .imageSharingMode = separate_queues ? vk::SharingMode::eConcurrent
                                        : vk::SharingMode::eExclusive,
    .queueFamilyIndexCount = separate_queues ? 2U : 0U,
    .pQueueFamilyIndices = separate_queues ? queue_families.data() : nullptr,
    .preTransform = capabilities.currentTransform,
    .compositeAlpha = choose_composite_alpha_(capabilities),
    .presentMode = choose_present_mode_(present_modes),
    .clipped = vk::True,
    .oldSwapchain = old_swapchain,
  };

  resources.swapchain = vk::raii::SwapchainKHR{context_.device, create_info};
  resources.images = resources.swapchain.getImages();

  auto const attachment = vk::AttachmentDescription{
    .format = resources.format,
    .samples = vk::SampleCountFlagBits::e1,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
    .initialLayout = vk::ImageLayout::eUndefined,
    .finalLayout = vk::ImageLayout::ePresentSrcKHR,
  };
  auto const color_attachment = vk::AttachmentReference{
    .attachment = 0,
    .layout = vk::ImageLayout::eColorAttachmentOptimal,
  };
  auto const subpass = vk::SubpassDescription{
    .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment,
  };
  auto const dependency = vk::SubpassDependency{
    .srcSubpass = VK_SUBPASS_EXTERNAL,
    .dstSubpass = 0,
    .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
    .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
  };
  auto const render_pass_info = vk::RenderPassCreateInfo{
    .attachmentCount = 1,
    .pAttachments = &attachment,
    .subpassCount = 1,
    .pSubpasses = &subpass,
    .dependencyCount = 1,
    .pDependencies = &dependency,
  };
  resources.render_pass =
    vk::raii::RenderPass{context_.device, render_pass_info};

  resources.image_views.reserve(resources.images.size());
  resources.framebuffers.reserve(resources.images.size());
  resources.render_finished.reserve(resources.images.size());
  for (auto const image : resources.images)
  {
    auto const view_info = vk::ImageViewCreateInfo{
      .image = image,
      .viewType = vk::ImageViewType::e2D,
      .format = resources.format,
      .subresourceRange = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };
    resources.image_views.emplace_back(context_.device, view_info);

    auto const image_view = *resources.image_views.back();
    auto const framebuffer_info = vk::FramebufferCreateInfo{
      .renderPass = *resources.render_pass,
      .attachmentCount = 1,
      .pAttachments = &image_view,
      .width = resources.extent.width,
      .height = resources.extent.height,
      .layers = 1,
    };
    resources.framebuffers.emplace_back(context_.device, framebuffer_info);
    resources.render_finished.emplace_back(
      context_.device, vk::SemaphoreCreateInfo{}
    );
  }

  return resources;
}

auto Renderer::update_swapchain_() -> bool
{
  auto const drawable_extent = drawable_extent_();
  if (!drawable_extent) return false;

  if (
    swapchain_ && resize_pending_ &&
    std::chrono::steady_clock::now() < resize_deadline_
  )
    return false;

  if (
    swapchain_ && !swapchain_invalid_ && swapchain_->extent == *drawable_extent
  )
  {
    resize_pending_ = false;
    return true;
  }

  auto const old_swapchain =
    swapchain_ ? *swapchain_->swapchain : vk::SwapchainKHR{};
  if (swapchain_) context_.device.waitIdle();

  auto replacement = create_swapchain_(*drawable_extent, old_swapchain);
  SDL_Log(
    "Vulkan swapchain ready: %ux%u (%zu images)", replacement.extent.width,
    replacement.extent.height, replacement.images.size()
  );
  swapchain_ = std::move(replacement);
  images_in_flight_.assign(swapchain_->images.size(), vk::Fence{});
  resize_pending_ = false;
  swapchain_invalid_ = false;
  return true;
}

auto Renderer::choose_surface_format_(
  std::span<vk::SurfaceFormatKHR const> formats
) const -> vk::SurfaceFormatKHR
{
  if (formats.size() == 1 && formats.front().format == vk::Format::eUndefined)
    return {
      .format = vk::Format::eB8G8R8A8Srgb,
      .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
    };

  auto const preferred = std::ranges::find_if(
    formats,
    [](auto const& format)
    {
      return format.format == vk::Format::eB8G8R8A8Srgb &&
        format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    }
  );
  return preferred != formats.end() ? *preferred : formats.front();
}

auto Renderer::choose_present_mode_(
  std::span<vk::PresentModeKHR const> present_modes
) const -> vk::PresentModeKHR
{
  static_cast<void>(present_modes);
  // FIFO is guaranteed and keeps the empty loop from running unbounded.
  return vk::PresentModeKHR::eFifo;
}

auto Renderer::choose_extent_(
  vk::SurfaceCapabilitiesKHR const& capabilities, vk::Extent2D drawable_extent
) const -> vk::Extent2D
{
  if (
    capabilities.currentExtent.width !=
    std::numeric_limits<std::uint32_t>::max()
  )
    return capabilities.currentExtent;

  return {
    .width = std::clamp(
      drawable_extent.width, capabilities.minImageExtent.width,
      capabilities.maxImageExtent.width
    ),
    .height = std::clamp(
      drawable_extent.height, capabilities.minImageExtent.height,
      capabilities.maxImageExtent.height
    ),
  };
}

auto Renderer::choose_composite_alpha_(
  vk::SurfaceCapabilitiesKHR const& capabilities
) const -> vk::CompositeAlphaFlagBitsKHR
{
  constexpr auto choices = std::array{
    vk::CompositeAlphaFlagBitsKHR::eOpaque,
    vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
    vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
    vk::CompositeAlphaFlagBitsKHR::eInherit,
  };
  auto const selected = std::ranges::find_if(
    choices, [&capabilities](auto choice)
    { return static_cast<bool>(capabilities.supportedCompositeAlpha & choice); }
  );
  if (selected == choices.end())
    throw std::runtime_error{"Vulkan surface has no composite alpha mode"};
  return *selected;
}
