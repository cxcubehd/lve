#pragma once

#include <SDL3/SDL_video.h>

#include "context.hh"

class Renderer
{
  public:
  Renderer() = default;

  public:
  SDL_Window* sdl_window{};

  protected:
  std::optional<RenderContext> context_;

  protected:
  vk::raii::Context vk_context_{};
  vk::raii::Instance vk_instance_{nullptr};

  vk::raii::SurfaceKHR vk_surface_{nullptr};

  vk::raii::PhysicalDevice vk_physical_device_{nullptr};
  vk::raii::Device vk_device_{nullptr};

  vk::raii::Queue vk_graphics_queue_{nullptr};
  vk::raii::Queue vk_present_queue_{nullptr};
  vk::raii::Queue vk_transfer_queue_{nullptr};

  std::uint32_t graphics_queue_family_{};
  std::uint32_t present_queue_family_{};
  std::uint32_t transfer_queue_family_{};

  vk::raii::SwapchainKHR vk_swapchain_{nullptr};

  vk::Format swapchain_format_{};
  vk::ColorSpaceKHR swapchain_color_space_{};
  vk::Extent2D swapchain_extent_{};

  public:
  auto init() -> void;

  protected:
  auto init_instance_() -> void;
  auto init_surface_() -> void;
  auto init_physical_device_() -> void;
  auto init_device_() -> void;
  auto init_queues_() -> void;
  auto init_swapchain_() -> void;

  protected:
  auto select_physical_device_() -> vk::raii::PhysicalDevice;
  auto is_physical_device_suitable_(
    vk::raii::PhysicalDevice const& physical_device
  ) -> bool;
  auto find_queue_families_(vk::raii::PhysicalDevice const& physical_device)
    -> void;

  auto choose_swapchain_format_(
    std::span<const vk::SurfaceFormatKHR> formats
  ) const -> vk::SurfaceFormatKHR;
  auto choose_swapchain_present_mode_(
    std::span<const vk::PresentModeKHR> present_modes
  ) const -> vk::PresentModeKHR;
  auto choose_swapchain_extent_(
    vk::SurfaceCapabilitiesKHR const& capabilities
  ) const -> vk::Extent2D;

  protected:
  static auto get_sdl_vk_instance_extensions_()
    -> std::tuple<char const* const*, std::size_t>;
};
