#include <print>

#include <SDL3/SDL_vulkan.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>

#include "app.hh"
#include "lve/log/log_steam.hh"

// SDL3 main callbacks
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>


SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{
#ifdef L_RENDER
  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    std::println("SDL_Init failed: {}", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_Vulkan_LoadLibrary(nullptr))
  {
    std::println("SDL_Vulkan_LoadLibrary failed: {}", SDL_GetError());
    return SDL_APP_FAILURE;
  }
#endif

  SteamDatagramErrMsg err_msg;
  if (!GameNetworkingSockets_Init(nullptr, err_msg))
  {
    std::println("GameNetworkingSockets_Init failed: {}", err_msg);
    return SDL_APP_FAILURE;
  }

#ifdef L_DEBUG
  log_steam_net_debug_init();
#endif

  const auto app = new App();

  // Initialize window
  if (const auto result = app->init_window(); !result)
  {
    std::println("Failed to initialize window: {}", result.error());
    return SDL_APP_FAILURE;
  }

  // Initialize renderer
  app->renderer = std::make_unique<Renderer>();
  app->renderer->sdl_window = app->sdl_window;
  app->renderer->init();

  *appstate = app;

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
  if (event->type == SDL_EVENT_QUIT)
  {
    // Shutdown
    return SDL_APP_SUCCESS;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) { return SDL_APP_CONTINUE; }

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
  delete static_cast<App*>(appstate);

  GameNetworkingSockets_Kill();

  std::println("SDL_AppQuit called with result: {}", (uint64_t)result);
}
