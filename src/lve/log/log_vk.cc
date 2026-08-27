#include "log_vk.hh"

#include <cstdio>
#include <print>

VKAPI_ATTR static auto VKAPI_CALL vulkan_debug_callback(
  vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
  vk::DebugUtilsMessageTypeFlagsEXT,
  vk::DebugUtilsMessengerCallbackDataEXT const* callback_data, void*
) noexcept -> vk::Bool32
{
  auto const* label =
    severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError     ? "error"
    : severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning ? "warning"
                                                                     : "info";
  auto const* message = callback_data && callback_data->pMessage
    ? callback_data->pMessage
    : "(no message)";

  try
  {
    std::println(stderr, "[Vulkan {}] {}", label, message);
  }
  catch (...)
  {
    // Exceptions must never cross the Vulkan callback boundary.
  }

  return vk::False;
}

auto vk_debug_messenger_create_info() noexcept
  -> vk::DebugUtilsMessengerCreateInfoEXT
{
  return {
    .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
    .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
    .pfnUserCallback = vulkan_debug_callback,
  };
}
