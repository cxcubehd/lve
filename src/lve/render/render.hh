#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <SDL3/SDL_video.h>

#include "context.hh"

struct FrameResources
{
  vk::raii::CommandPool command_pool{nullptr};
  vk::raii::CommandBuffer command_buffer{nullptr};

  vk::raii::Semaphore image_available{nullptr};
  vk::raii::Fence render_complete{nullptr};
};

struct SwapchainResources
{
  vk::raii::SwapchainKHR swapchain{nullptr};
  std::vector<vk::Image> images{};

  std::vector<vk::raii::ImageView> image_views{};
  vk::raii::RenderPass render_pass{nullptr};
  std::vector<vk::raii::Framebuffer> framebuffers{};

  std::vector<vk::raii::Semaphore> render_finished{};

  vk::Format format{vk::Format::eUndefined};
  vk::ColorSpaceKHR color_space{vk::ColorSpaceKHR::eSrgbNonlinear};
  vk::Extent2D extent{};
};

class Renderer final
{
  public:
  explicit Renderer(SDL_Window& window);
  ~Renderer() noexcept;

  Renderer(Renderer const&) = delete;
  auto operator=(Renderer const&) -> Renderer& = delete;
  Renderer(Renderer&&) = delete;
  auto operator=(Renderer&&) -> Renderer& = delete;

  auto render_frame() -> void;

  // Resize notifications are deliberately cheap. The render loop consumes
  // the latest size after a short quiet period, coalescing resize event bursts.
  auto request_resize() noexcept -> void;

  private:
  static constexpr std::size_t frames_in_flight_ = 2;
  static constexpr auto resize_settle_time_ = std::chrono::milliseconds{100};

  SDL_Window& window_;

  RenderContext context_{};

  std::array<FrameResources, frames_in_flight_> frames_{};
  std::optional<SwapchainResources> swapchain_{};
  std::vector<vk::Fence> images_in_flight_{};

  std::size_t current_frame_{};

  bool debug_utils_enabled_{};
  bool resize_pending_{true};
  bool swapchain_invalid_{};

  std::chrono::steady_clock::time_point resize_deadline_{};

  auto init_vulkan_() -> void;
  auto init_instance_() -> void;
  auto init_debug_messenger_() -> void;
  auto init_surface_() -> void;
  auto init_physical_device_() -> void;
  auto init_device_() -> void;
  auto init_frames_() -> void;

  [[nodiscard]] auto select_physical_device_() -> vk::raii::PhysicalDevice;
  [[nodiscard]] auto find_queue_families_(
    vk::raii::PhysicalDevice const& physical_device
  ) const -> std::optional<std::pair<std::uint32_t, std::uint32_t>>;
  [[nodiscard]] auto supports_required_device_features_(
    vk::raii::PhysicalDevice const& physical_device
  ) const -> bool;

  [[nodiscard]] auto drawable_extent_() const -> std::optional<vk::Extent2D>;
  [[nodiscard]] auto create_swapchain_(
    vk::Extent2D drawable_extent, vk::SwapchainKHR old_swapchain
  ) -> SwapchainResources;
  [[nodiscard]] auto create_render_pass_(vk::Format format)
    -> vk::raii::RenderPass;
  auto init_swapchain_image_resources_(SwapchainResources& resources) -> void;

  auto update_swapchain_() -> bool;

  auto draw_frame_() -> void;
  auto record_empty_frame_(
    vk::raii::CommandBuffer const& command_buffer, vk::Framebuffer framebuffer,
    vk::RenderPass render_pass, vk::Extent2D extent
  ) const -> void;

  [[nodiscard]] auto choose_surface_format_(
    std::span<vk::SurfaceFormatKHR const> formats
  ) const -> vk::SurfaceFormatKHR;
  [[nodiscard]] auto choose_present_mode_(
    std::span<vk::PresentModeKHR const> present_modes
  ) const -> vk::PresentModeKHR;
  [[nodiscard]] auto choose_extent_(
    vk::SurfaceCapabilitiesKHR const& capabilities, vk::Extent2D drawable_extent
  ) const -> vk::Extent2D;
  [[nodiscard]] auto choose_composite_alpha_(
    vk::SurfaceCapabilitiesKHR const& capabilities
  ) const -> vk::CompositeAlphaFlagBitsKHR;
};
