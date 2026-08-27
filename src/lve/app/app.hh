#pragma once

#include <expected>
#include <memory>
#include <string>

#include <SDL3/SDL.h>

#include "lve/render/render.hh"

class App final
{
  public:
  App() = default;
  ~App() noexcept;

  App(App const&) = delete;
  auto operator=(App const&) -> App& = delete;

  [[nodiscard]] auto init() -> std::expected<void, std::string>;
  [[nodiscard]] auto iterate() -> std::expected<void, std::string>;
  auto handle_event(SDL_Event const& event) noexcept -> void;

  private:
  [[nodiscard]] auto init_window_() -> std::expected<void, std::string>;

  SDL_Window* window_{};
  std::unique_ptr<Renderer> renderer_{};
};
