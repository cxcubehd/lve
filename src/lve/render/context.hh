#pragma once

#include <cstdint>

#include <vulkan/vulkan_raii.hpp>

struct RenderContext
{
  vk::raii::Context loader{};
  vk::raii::Instance instance{nullptr};
  vk::raii::DebugUtilsMessengerEXT debug_messenger{nullptr};
  vk::raii::SurfaceKHR surface{nullptr};
  vk::raii::PhysicalDevice physical_device{nullptr};
  vk::raii::Device device{nullptr};
  vk::raii::Queue graphics_queue{nullptr};
  vk::raii::Queue present_queue{nullptr};

  std::uint32_t graphics_queue_family{};
  std::uint32_t present_queue_family{};
};
