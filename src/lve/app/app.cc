#include "app.hh"

#include <format>

App::~App() noexcept
{
  // Vulkan objects and their surface must die before the SDL window.
  renderer_.reset();
  if (window_) SDL_DestroyWindow(window_);
}

auto App::init() -> std::expected<void, std::string>
{
  if (auto const initialized = init_window_(); !initialized)
    return std::unexpected{initialized.error()};

  renderer_ = std::make_unique<Renderer>(*window_);
  if (auto const initialized = renderer_->init(); !initialized)
    return std::unexpected{initialized.error()};

  return {};
}

auto App::iterate() -> std::expected<void, std::string>
{
  return renderer_->render_frame();
}

auto App::handle_event(SDL_Event const& event) noexcept -> void
{
  switch (event.type)
  {
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
      if (!window_ || event.window.windowID != SDL_GetWindowID(window_)) return;
      renderer_->request_resize();
      break;
    default:
      break;
  }
}

auto App::init_window_() -> std::expected<void, std::string>
{
  window_ = SDL_CreateWindow(
    "lve", 1000, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
  );
  if (!window_)
    return std::unexpected{
      std::format("Failed to create SDL window: {}", SDL_GetError())
    };

  return {};
}
