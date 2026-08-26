#include <print>

#include <SDL3/SDL_vulkan.h>

#include "app.hh"

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


  const auto window = SDL_CreateWindow(
    "lve", 1000, 650, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
  );

  if (!window)
  {
    std::println("SDL_CreateWindow failed: {}", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  *appstate = new App();

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

  std::println("SDL_AppQuit called with result: {}", (uint64_t)result);
}
