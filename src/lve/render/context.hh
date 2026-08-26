#pragma once

#include <vulkan/vulkan_raii.hpp>

struct RenderContext
{
  vk::raii::Instance instance;
  vk::raii::Context context;
  vk::raii::PhysicalDevice physical_device;
  vk::raii::Device device;

  vk::Queue graphics_queue;
  vk::Queue present_queue;

  vk::SurfaceKHR surface;
};
