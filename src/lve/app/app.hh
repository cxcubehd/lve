#pragma once

#include <expected>
#include <memory>
#include <string_view>

#include <SDL3/SDL.h>

#include "lve/render/render.hh"

class App
{
  public:
  std::unique_ptr<Renderer> renderer{};

  public:
  SDL_Window* sdl_window{};

  public:
  auto init_window() -> std::expected<void, std::string_view>;
};
