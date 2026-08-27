#include "render.hh"

#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>

#include <SDL3/SDL_timer.h>

static constexpr auto gpu_wait_timeout = std::chrono::seconds{1};
static constexpr auto image_acquire_timeout = std::chrono::milliseconds{100};
static constexpr auto idle_retry_delay_ms = 10U;

auto Renderer::draw_frame_() -> void
{
  if ((resize_pending_ || !swapchain_) && !update_swapchain_())
  {
    SDL_Delay(idle_retry_delay_ms);
    return;
  }

  auto& frame = frames_[current_frame_];
  auto const frame_fence = *frame.render_complete;
  auto const frame_wait = context_.device.waitForFences(
    std::array{frame_fence}, vk::True,
    static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(gpu_wait_timeout)
        .count()
    )
  );
  if (frame_wait == vk::Result::eTimeout)
    throw std::runtime_error{"Timed out waiting for a Vulkan frame fence"};

  auto const acquired = swapchain_->swapchain.acquireNextImage(
    static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
        image_acquire_timeout
      )
        .count()
    ),
    *frame.image_available
  );
  if (
    acquired.result == vk::Result::eTimeout ||
    acquired.result == vk::Result::eNotReady
  )
    return;
  if (acquired.result == vk::Result::eErrorOutOfDateKHR)
  {
    swapchain_invalid_ = true;
    request_resize();
    return;
  }
  auto const swapchain_suboptimal =
    acquired.result == vk::Result::eSuboptimalKHR;
  auto const image_index = acquired.value;
  if (image_index >= swapchain_->framebuffers.size())
    throw std::runtime_error{
      "Vulkan acquired an invalid swapchain image index"
    };

  auto const image_fence = images_in_flight_[image_index];
  if (image_fence && image_fence != frame_fence)
  {
    auto const image_wait = context_.device.waitForFences(
      std::array{image_fence}, vk::True,
      static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(gpu_wait_timeout)
          .count()
      )
    );
    if (image_wait == vk::Result::eTimeout)
      throw std::runtime_error{"Timed out waiting for a swapchain image fence"};
  }

  frame.command_buffer.reset();
  record_empty_frame_(
    frame.command_buffer, *swapchain_->framebuffers[image_index],
    *swapchain_->render_pass, swapchain_->extent
  );
  context_.device.resetFences(std::array{frame_fence});

  auto const wait_semaphore = *frame.image_available;
  auto const wait_stage =
    vk::PipelineStageFlags{vk::PipelineStageFlagBits::eColorAttachmentOutput};
  auto const command_buffer = *frame.command_buffer;
  auto const signal_semaphore = *swapchain_->render_finished[image_index];
  auto const submit_info = vk::SubmitInfo{
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &wait_semaphore,
    .pWaitDstStageMask = &wait_stage,
    .commandBufferCount = 1,
    .pCommandBuffers = &command_buffer,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &signal_semaphore,
  };
  context_.graphics_queue.submit(submit_info, frame_fence);
  images_in_flight_[image_index] = frame_fence;

  auto const swapchain_handle = *swapchain_->swapchain;
  auto const present_info = vk::PresentInfoKHR{
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &signal_semaphore,
    .swapchainCount = 1,
    .pSwapchains = &swapchain_handle,
    .pImageIndices = &image_index,
  };
  auto const present_result = context_.present_queue.presentKHR(present_info);

  current_frame_ = (current_frame_ + 1) % frames_in_flight_;
  if (
    swapchain_suboptimal || present_result == vk::Result::eSuboptimalKHR ||
    present_result == vk::Result::eErrorOutOfDateKHR
  )
  {
    swapchain_invalid_ = true;
    request_resize();
  }
}

auto Renderer::record_empty_frame_(
  vk::raii::CommandBuffer const& command_buffer, vk::Framebuffer framebuffer,
  vk::RenderPass render_pass, vk::Extent2D extent
) const -> void
{
  command_buffer.begin(
    vk::CommandBufferBeginInfo{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    }
  );

  auto clear_value = vk::ClearValue{};
  clear_value.color.float32[0] = 0.015F;
  clear_value.color.float32[1] = 0.02F;
  clear_value.color.float32[2] = 0.03F;
  clear_value.color.float32[3] = 1.0F;
  auto const render_pass_info = vk::RenderPassBeginInfo{
    .renderPass = render_pass,
    .framebuffer = framebuffer,
    .renderArea =
      {
        .offset = {.x = 0, .y = 0},
        .extent = extent,
      },
    .clearValueCount = 1,
    .pClearValues = &clear_value,
  };
  command_buffer.beginRenderPass(
    render_pass_info, vk::SubpassContents::eInline
  );

  command_buffer.endRenderPass();
  command_buffer.end();
}
