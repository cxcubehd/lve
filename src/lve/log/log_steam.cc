#include "log_steam.hh"

#include <cstdio>
#include <print>

#include <steam/isteamnetworkingutils.h>

static auto log_steam_message(
  ESteamNetworkingSocketsDebugOutputType type, char const* message
) noexcept -> void
{
  try
  {
    std::println(
      stderr, "[GameNetworkingSockets:{}] {}", static_cast<int>(type),
      message ? message : "(no message)"
    );
  }
  catch (...)
  {
    // Exceptions must never cross the networking callback boundary.
  }
}

auto init_steam_debug_log() -> void
{
  SteamNetworkingUtils()->SetDebugOutputFunction(
    k_ESteamNetworkingSocketsDebugOutputType_Msg, log_steam_message
  );
}
