#include <cstdint>
#include <exception>
#include <memory>
#include <print>

#include <SDL3/SDL_vulkan.h>
#include <steam/steamnetworkingsockets.h>

#include "app.hh"
#include "lve/log/log_steam.hh"

// SDL3 main callbacks
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{
  *appstate = nullptr;
  static_cast<void>(argc);
  static_cast<void>(argv);

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
  init_steam_debug_log();
#endif

  try
  {
    auto app = std::make_unique<App>();
    if (auto const initialized = app->init(); !initialized)
    {
      std::println(
        stderr, "Application initialization failed: {}", initialized.error()
      );
      return SDL_APP_FAILURE;
    }

    *appstate = app.release();
  }
  catch (std::exception const& error)
  {
    std::println(stderr, "Application initialization failed: {}", error.what());
    return SDL_APP_FAILURE;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
  if (event->type == SDL_EVENT_QUIT)
  {
    return SDL_APP_SUCCESS;
  }

  if (appstate) static_cast<App*>(appstate)->handle_event(*event);

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
  if (!appstate)
  {
    std::println(stderr, "Application state is unavailable");
    return SDL_APP_FAILURE;
  }

  if (auto const rendered = static_cast<App*>(appstate)->iterate(); !rendered)
  {
    std::println(stderr, "{}", rendered.error());
    return SDL_APP_FAILURE;
  }

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
  delete static_cast<App*>(appstate);

  GameNetworkingSockets_Kill();

#ifdef L_RENDER
  SDL_Vulkan_UnloadLibrary();
  SDL_Quit();
#endif

  std::println(
    "SDL_AppQuit called with result: {}", static_cast<std::uint64_t>(result)
  );
}
