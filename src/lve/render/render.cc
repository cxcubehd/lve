#include "render.hh"

#include <ranges>
#include <set>

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

auto Renderer::init() -> void
{
  // Init VulkanHpp
  VULKAN_HPP_DEFAULT_DISPATCHER.init();

  init_instance_();

  init_surface_();

  init_physical_device_();

  init_device_();

  init_queues_();

  init_swapchain_();
}

auto Renderer::init_instance_() -> void
{
  const auto [extensions, extensionCount] = get_sdl_vk_instance_extensions();

  std::vector<const char*> layers;

#ifdef L_DEBUG
  layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  const auto appInfo = vk::ApplicationInfo{
    .pApplicationName = "lve",
    .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
    .pEngineName = "lve",
    .engineVersion = VK_MAKE_VERSION(0, 0, 1),
    .apiVersion = VK_API_VERSION_1_2,
  };

  const auto createInfo = vk::InstanceCreateInfo{
    .pApplicationInfo = &appInfo,
    .enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
    .ppEnabledLayerNames = layers.data(),
    .enabledExtensionCount = static_cast<std::uint32_t>(extensionCount),
    .ppEnabledExtensionNames = extensions,
  };

  try
  {
    vk_instance_ = vk::raii::Instance(vk_context_, createInfo);
  }
  catch (const vk::SystemError& e)
  {
    throw std::runtime_error(
      std::format("Failed to create instance: {}", e.what())
    );
  }

  VULKAN_HPP_DEFAULT_DISPATCHER.init(*vk_instance_);
}

auto Renderer::init_surface_() -> void
{
  if (!sdl_window) throw std::runtime_error("SDL window is not initialized!");

  VkSurfaceKHR surface{};
  if (!SDL_Vulkan_CreateSurface(sdl_window, *vk_instance_, nullptr, &surface))
    throw std::runtime_error("Failed to create surface!");

  vk_surface_ = vk::raii::SurfaceKHR(vk_instance_, surface);
}

auto Renderer::init_physical_device_() -> void
{
  vk_physical_device_ = select_physical_device_();

  find_queue_families_(vk_physical_device_);
}

auto Renderer::init_device_() -> void
{
  constexpr float queue_priority = 1.0f;

  std::set<uint32_t> unique_queue_families{
    graphics_queue_family_,
    present_queue_family_,
    transfer_queue_family_,
  };

  std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;

  for (const auto family : unique_queue_families)
  {
    queue_create_infos.emplace_back(
      vk::DeviceQueueCreateInfo{
        .queueFamilyIndex = family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
      }
    );
  }

  vk::PhysicalDeviceFeatures features{};

  const vk::DeviceCreateInfo create_info{
    .queueCreateInfoCount =
      static_cast<std::uint32_t>(queue_create_infos.size()),
    .pQueueCreateInfos = queue_create_infos.data(),
    .pEnabledFeatures = &features,
  };

  try
  {
    vk_device_ = vk_physical_device_.createDevice(create_info);
  }
  catch (const vk::SystemError& e)
  {
    throw std::runtime_error(
      std::format("Failed to create logical device: {}", e.what())
    );
  }

  VULKAN_HPP_DEFAULT_DISPATCHER.init(*vk_device_);
}

auto Renderer::init_queues_() -> void
{
  vk_graphics_queue_ = vk_device_.getQueue(graphics_queue_family_, 0);
  vk_present_queue_ = vk_device_.getQueue(present_queue_family_, 0);
  vk_transfer_queue_ = vk_device_.getQueue(transfer_queue_family_, 0);
}

auto Renderer::init_swapchain_() -> void
{
  const auto capabilities =
    vk_physical_device_.getSurfaceCapabilitiesKHR(*vk_surface_);

  const auto formats = vk_physical_device_.getSurfaceFormatsKHR(*vk_surface_);

  const auto present_modes =
    vk_physical_device_.getSurfacePresentModesKHR(*vk_surface_);

  const auto format = choose_swapchain_format_(formats);

  swapchain_format_ = format.format;
  swapchain_color_space_ = format.colorSpace;
  swapchain_extent_ = choose_swapchain_extent_(capabilities);

  const auto image_count = std::min(
    capabilities.minImageCount + 1,
    capabilities.maxImageCount != 0 ? capabilities.maxImageCount
                                    : capabilities.minImageCount + 1
  );

  const vk::SwapchainCreateInfoKHR create_info{
    .surface = *vk_surface_,
    .minImageCount = image_count,
    .imageFormat = swapchain_format_,
    .imageColorSpace = swapchain_color_space_,
    .imageExtent = swapchain_extent_,
    .imageArrayLayers = 1,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
    .imageSharingMode = vk::SharingMode::eExclusive,
    .preTransform = capabilities.currentTransform,
    .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
    .presentMode = choose_swapchain_present_mode_(present_modes),
    .clipped = vk::True,
  };

  vk_swapchain_ = vk::raii::SwapchainKHR{vk_device_, create_info};
}

auto Renderer::select_physical_device_() -> vk::raii::PhysicalDevice
{
  const auto physical_devices = vk_instance_.enumeratePhysicalDevices();

  if (physical_devices.empty())
    throw std::runtime_error("(Renderer) No Vulkan physical devices found");

  for (const auto& physical_device : physical_devices)
  {
    if (is_physical_device_suitable_(physical_device)) return physical_device;
  }

  throw std::runtime_error(
    "(Renderer) Failed to find a suitable physical device"
  );
}

auto Renderer::is_physical_device_suitable_(
  vk::raii::PhysicalDevice const& physical_device
) -> bool
{
  const auto properties = physical_device.getProperties();

  if (properties.apiVersion < VK_API_VERSION_1_2) return false;

  // Queue families are checked separately.
  const auto queue_families = physical_device.getQueueFamilyProperties();

  bool has_graphics = false;
  bool has_present = false;
  bool has_dedicated_transfer = false;

  for (std::uint32_t i = 0; i < queue_families.size(); ++i)
  {
    const auto flags = queue_families[i].queueFlags;

    if (flags & vk::QueueFlagBits::eGraphics) has_graphics = true;

    if (physical_device.getSurfaceSupportKHR(i, *vk_surface_))
      has_present = true;

    if (
      (flags & vk::QueueFlagBits::eTransfer) &&
      !(flags & vk::QueueFlagBits::eGraphics) &&
      !(flags & vk::QueueFlagBits::eCompute)
    )
    {
      has_dedicated_transfer = true;
    }
  }

  return has_graphics && has_present && has_dedicated_transfer;
}

auto Renderer::find_queue_families_(
  vk::raii::PhysicalDevice const& physical_device
) -> void
{
  const auto queue_families = physical_device.getQueueFamilyProperties();

  std::optional<std::uint32_t> graphics;
  std::optional<std::uint32_t> present;
  std::optional<std::uint32_t> transfer;

  for (std::uint32_t i = 0; i < queue_families.size(); ++i)
  {
    const auto flags = queue_families[i].queueFlags;

    if (!graphics && flags & vk::QueueFlagBits::eGraphics)
    {
      graphics = i;
    }

    if (!present && physical_device.getSurfaceSupportKHR(i, *vk_surface_))
    {
      present = i;
    }

    if (
      !transfer && flags & vk::QueueFlagBits::eTransfer &&
      !(flags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute))
    )
    {
      transfer = i;
    }
  }

  if (!graphics || !present || !transfer)
  {
    throw std::runtime_error(
      "(Renderer) Failed to find required queue families"
    );
  }

  graphics_queue_family_ = *graphics;
  present_queue_family_ = *present;
  transfer_queue_family_ = *transfer;
}

auto Renderer::choose_swapchain_format_(
  std::span<const vk::SurfaceFormatKHR> formats
) const -> vk::SurfaceFormatKHR
{
  const auto it = std::ranges::find_if(
    formats,
    [](auto const& format)
    {
      return format.format == vk::Format::eB8G8R8A8Srgb &&
        format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    }
  );

  return it != formats.end() ? *it : formats.front();
}

auto Renderer::choose_swapchain_present_mode_(
  std::span<const vk::PresentModeKHR> present_modes
) const -> vk::PresentModeKHR
{
  // FIFO is guaranteed by Vulkan and is generally the sensible default.
  return vk::PresentModeKHR::eFifo;
}

auto Renderer::choose_swapchain_extent_(
  vk::SurfaceCapabilitiesKHR const& capabilities
) const -> vk::Extent2D
{
  if (
    capabilities.currentExtent.width !=
    std::numeric_limits<std::uint32_t>::max()
  )
    return capabilities.currentExtent;

  int width{};
  int height{};

  SDL_GetWindowSizeInPixels(sdl_window, &width, &height);

  return {
    .width = std::clamp(
      static_cast<std::uint32_t>(width), capabilities.minImageExtent.width,
      capabilities.maxImageExtent.width
    ),
    .height = std::clamp(
      static_cast<std::uint32_t>(height), capabilities.minImageExtent.height,
      capabilities.maxImageExtent.height
    ),
  };
}

auto Renderer::get_sdl_vk_instance_extensions()
  -> std::tuple<char const* const*, std::size_t>
{
  std::uint32_t vk_extension_count{};
  const auto vk_extensions =
    SDL_Vulkan_GetInstanceExtensions(&vk_extension_count);

  if (!vk_extensions)
    throw std::runtime_error("Failed to get SDL vk instance extensions");

  return {vk_extensions, vk_extension_count};
}
