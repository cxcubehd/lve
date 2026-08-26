#include "app.hh"

#include <assert.h>

auto App::init_window() -> std::expected<void, std::string_view>
{
  sdl_window = SDL_CreateWindow(
    "lve", 1000, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
  );

  if (!sdl_window) return std::unexpected<std::string_view>(SDL_GetError());
  return {};
}
