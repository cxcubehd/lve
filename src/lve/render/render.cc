#include "render.hh"

#include <chrono>
#include <format>
#include <ranges>
#include <stdexcept>
#include <string_view>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "lve/log/log_vk.hh"

static constexpr std::string_view validation_layer =
  "VK_LAYER_KHRONOS_validation";
static constexpr std::string_view portability_subset =
  "VK_KHR_portability_subset";

template <typename Property>
static auto has_named_property(
  std::span<Property const> properties, std::string_view name
) -> bool
{
  return std::ranges::any_of(
    properties,
    [name](Property const& property)
    {
      if constexpr (requires { property.extensionName; })
        return std::string_view{property.extensionName.data()} == name;
      else return std::string_view{property.layerName.data()} == name;
    }
  );
}

Renderer::Renderer(SDL_Window& window) noexcept : window_{window} {}

Renderer::~Renderer() noexcept
{
  if (!initialized_) return;

  try
  {
    context_.device.waitIdle();
  }
  catch (std::exception const& error)
  {
    SDL_LogError(
      SDL_LOG_CATEGORY_RENDER, "Vulkan shutdown wait failed: %s", error.what()
    );
  }
}

auto Renderer::init() noexcept -> std::expected<void, std::string>
{
  try
  {
    init_vulkan_();
    initialized_ = true;
    return {};
  }
  catch (std::exception const& error)
  {
    return std::unexpected{
      std::format("Failed to initialize Vulkan renderer: {}", error.what())
    };
  }
}

auto Renderer::render_frame() noexcept -> std::expected<void, std::string>
{
  if (!initialized_)
    return std::unexpected{"Renderer used before successful initialization"};

  try
  {
    draw_frame_();
    return {};
  }
  catch (std::exception const& error)
  {
    return std::unexpected{
      std::format("Failed to render Vulkan frame: {}", error.what())
    };
  }
}

auto Renderer::request_resize() noexcept -> void
{
  resize_pending_ = true;
  resize_deadline_ = std::chrono::steady_clock::now() + resize_settle_time_;
}

auto Renderer::init_vulkan_() -> void
{
  VULKAN_HPP_DEFAULT_DISPATCHER.init();

  init_instance_();
  VULKAN_HPP_DEFAULT_DISPATCHER.init(*context_.instance);

  init_debug_messenger_();
  init_surface_();
  init_physical_device_();
  init_device_();
  VULKAN_HPP_DEFAULT_DISPATCHER.init(*context_.device);
  init_frames_();

  // A minimized or hidden window has no drawable extent. In that case the
  // first usable SDL iteration creates the swapchain without blocking here.
  update_swapchain_();
}

auto Renderer::init_instance_() -> void
{
  auto extension_count = std::uint32_t{};
  auto const* const* sdl_extensions =
    SDL_Vulkan_GetInstanceExtensions(&extension_count);
  if (!sdl_extensions)
    throw std::runtime_error{
      std::format("SDL Vulkan extension query failed: {}", SDL_GetError())
    };

  auto enabled_extensions =
    std::vector<char const*>{sdl_extensions, sdl_extensions + extension_count};
  auto const available_extensions =
    context_.loader.enumerateInstanceExtensionProperties();

  auto instance_flags = vk::InstanceCreateFlags{};
  if (
    has_named_property(
      std::span{available_extensions},
      VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    )
  )
  {
    enabled_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    instance_flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
  }

  auto enabled_layers = std::vector<char const*>{};
#ifdef L_DEBUG
  auto const available_layers =
    context_.loader.enumerateInstanceLayerProperties();
  auto const validation_enabled =
    has_named_property(std::span{available_layers}, validation_layer);
  if (validation_enabled) enabled_layers.push_back(validation_layer.data());
  else
    SDL_LogWarn(
      SDL_LOG_CATEGORY_RENDER,
      "Vulkan validation layer is unavailable; continuing without it"
    );

  debug_utils_enabled_ = has_named_property(
    std::span{available_extensions}, VK_EXT_DEBUG_UTILS_EXTENSION_NAME
  );
  if (debug_utils_enabled_)
    enabled_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  auto const application_info = vk::ApplicationInfo{
    .pApplicationName = "lve",
    .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
    .pEngineName = "lve",
    .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
    .apiVersion = VK_API_VERSION_1_2,
  };
  auto const debug_info = vk_debug_messenger_create_info();
  auto const create_info = vk::InstanceCreateInfo{
    .pNext = debug_utils_enabled_ ? &debug_info : nullptr,
    .flags = instance_flags,
    .pApplicationInfo = &application_info,
    .enabledLayerCount = static_cast<std::uint32_t>(enabled_layers.size()),
    .ppEnabledLayerNames = enabled_layers.data(),
    .enabledExtensionCount =
      static_cast<std::uint32_t>(enabled_extensions.size()),
    .ppEnabledExtensionNames = enabled_extensions.data(),
  };

  context_.instance = vk::raii::Instance{context_.loader, create_info};
}

auto Renderer::init_debug_messenger_() -> void
{
  if (!debug_utils_enabled_) return;
  context_.debug_messenger = vk::raii::DebugUtilsMessengerEXT{
    context_.instance, vk_debug_messenger_create_info()
  };
}

auto Renderer::init_surface_() -> void
{
  auto surface = VkSurfaceKHR{};
  if (!SDL_Vulkan_CreateSurface(
        &window_, *context_.instance, nullptr, &surface
      ))
    throw std::runtime_error{
      std::format("SDL Vulkan surface creation failed: {}", SDL_GetError())
    };

  context_.surface = vk::raii::SurfaceKHR{context_.instance, surface};
}

auto Renderer::init_physical_device_() -> void
{
  context_.physical_device = select_physical_device_();

  auto const queue_families = find_queue_families_(context_.physical_device);
  if (!queue_families)
    throw std::runtime_error{"Selected Vulkan device lost required queues"};

  context_.graphics_queue_family = queue_families->first;
  context_.present_queue_family = queue_families->second;
}

auto Renderer::init_device_() -> void
{
  constexpr auto queue_priority = 1.0F;
  auto queue_create_infos = std::vector<vk::DeviceQueueCreateInfo>{};
  queue_create_infos.push_back({
    .queueFamilyIndex = context_.graphics_queue_family,
    .queueCount = 1,
    .pQueuePriorities = &queue_priority,
  });
  if (context_.present_queue_family != context_.graphics_queue_family)
    queue_create_infos.push_back({
      .queueFamilyIndex = context_.present_queue_family,
      .queueCount = 1,
      .pQueuePriorities = &queue_priority,
    });

  auto enabled_extensions =
    std::vector<char const*>{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  auto const available_extensions =
    context_.physical_device.enumerateDeviceExtensionProperties();
  if (has_named_property(std::span{available_extensions}, portability_subset))
    enabled_extensions.push_back(portability_subset.data());

  auto const features = vk::PhysicalDeviceFeatures{};
  auto const create_info = vk::DeviceCreateInfo{
    .queueCreateInfoCount =
      static_cast<std::uint32_t>(queue_create_infos.size()),
    .pQueueCreateInfos = queue_create_infos.data(),
    .enabledExtensionCount =
      static_cast<std::uint32_t>(enabled_extensions.size()),
    .ppEnabledExtensionNames = enabled_extensions.data(),
    .pEnabledFeatures = &features,
  };

  context_.device = context_.physical_device.createDevice(create_info);
  context_.graphics_queue =
    context_.device.getQueue(context_.graphics_queue_family, 0);
  context_.present_queue =
    context_.device.getQueue(context_.present_queue_family, 0);
}

auto Renderer::init_frames_() -> void
{
  auto const command_pool_info = vk::CommandPoolCreateInfo{
    .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
    .queueFamilyIndex = context_.graphics_queue_family,
  };
  auto const semaphore_info = vk::SemaphoreCreateInfo{};
  auto const fence_info = vk::FenceCreateInfo{
    .flags = vk::FenceCreateFlagBits::eSignaled,
  };

  for (auto& frame : frames_)
  {
    frame.command_pool =
      vk::raii::CommandPool{context_.device, command_pool_info};

    auto const allocate_info = vk::CommandBufferAllocateInfo{
      .commandPool = *frame.command_pool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
    };
    auto command_buffers =
      context_.device.allocateCommandBuffers(allocate_info);
    frame.command_buffer = std::move(command_buffers.front());
    frame.image_available =
      vk::raii::Semaphore{context_.device, semaphore_info};
    frame.render_complete = vk::raii::Fence{context_.device, fence_info};
  }
}

auto Renderer::select_physical_device_() -> vk::raii::PhysicalDevice
{
  auto physical_devices = context_.instance.enumeratePhysicalDevices();
  if (physical_devices.empty())
    throw std::runtime_error{"No Vulkan physical devices were found"};

  auto const selected = std::ranges::find_if(
    physical_devices, [this](auto const& physical_device)
    { return is_physical_device_suitable_(physical_device); }
  );
  if (selected == physical_devices.end())
    throw std::runtime_error{
      "No Vulkan 1.2 device with graphics, presentation, and swapchain support "
      "was found"
    };

  return std::move(*selected);
}

auto Renderer::find_queue_families_(
  vk::raii::PhysicalDevice const& physical_device
) const -> std::optional<std::pair<std::uint32_t, std::uint32_t>>
{
  auto const properties = physical_device.getQueueFamilyProperties();
  auto graphics = std::optional<std::uint32_t>{};
  auto present = std::optional<std::uint32_t>{};

  for (auto index = std::uint32_t{}; index < properties.size(); ++index)
  {
    auto const supports_graphics = static_cast<bool>(
      properties[index].queueFlags & vk::QueueFlagBits::eGraphics
    );
    auto const supports_present =
      physical_device.getSurfaceSupportKHR(index, *context_.surface);

    if (supports_graphics && supports_present) return {{index, index}};
    if (supports_graphics && !graphics) graphics = index;
    if (supports_present && !present) present = index;
  }

  if (graphics && present) return {{*graphics, *present}};
  return std::nullopt;
}

auto Renderer::is_physical_device_suitable_(
  vk::raii::PhysicalDevice const& physical_device
) const -> bool
{
  if (physical_device.getProperties().apiVersion < VK_API_VERSION_1_2)
    return false;
  if (!find_queue_families_(physical_device)) return false;

  auto const extensions = physical_device.enumerateDeviceExtensionProperties();
  if (!has_named_property(
        std::span{extensions}, VK_KHR_SWAPCHAIN_EXTENSION_NAME
      ))
    return false;

  auto const capabilities =
    physical_device.getSurfaceCapabilitiesKHR(*context_.surface);
  if (!(capabilities.supportedUsageFlags &
        vk::ImageUsageFlagBits::eColorAttachment))
    return false;

  return !physical_device.getSurfaceFormatsKHR(*context_.surface).empty() &&
    !physical_device.getSurfacePresentModesKHR(*context_.surface).empty();
}
