#include "app.hh"

#include <format>
#include <stdexcept>

App::~App() noexcept
{
  // Vulkan objects and their surface must die before the SDL window.
  renderer_.reset();

  if (window_) SDL_DestroyWindow(window_);
}

auto App::init() -> void
{
  init_window_();

  renderer_ = std::make_unique<Renderer>(*window_);
}

auto App::iterate() -> void { renderer_->render_frame(); }

auto App::handle_event(SDL_Event const& event) noexcept -> void
{
  switch (event.type)
  {
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_HIDDEN:
    case SDL_EVENT_WINDOW_SHOWN:
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
      if (event.window.windowID != SDL_GetWindowID(window_)) return;

      renderer_->request_resize();
      break;
    default:
      break;
  }
}

auto App::init_window_() -> void
{
  window_ = SDL_CreateWindow(
    "lve", 1000, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
  );
  if (!window_)
    throw std::runtime_error{
      std::format("Failed to create SDL window: {}", SDL_GetError())
    };
}
