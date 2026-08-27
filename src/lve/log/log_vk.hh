#pragma once

#include <vulkan/vulkan.hpp>

[[nodiscard]] auto vk_debug_messenger_create_info() noexcept
  -> vk::DebugUtilsMessengerCreateInfoEXT;
