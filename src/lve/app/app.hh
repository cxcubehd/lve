#pragma once

#include <memory>

#include <SDL3/SDL.h>

#include "lve/render/render.hh"

class App final
{
  public:
  App() = default;
  ~App() noexcept;

  App(App const&) = delete;
  auto operator=(App const&) -> App& = delete;

  auto init() -> void;
  auto iterate() -> void;
  auto handle_event(SDL_Event const& event) noexcept -> void;

  private:
  SDL_Window* window_{};

  std::unique_ptr<Renderer> renderer_{};
};
