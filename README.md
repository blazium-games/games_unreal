# games_unreal

Unreal plugin `BlaziumGames` for [Blazium Games](https://blazium.games), a game publishing platform operated by Divine Games, Inc. It covers init, callbacks, the logged-on user, lobbies, a Steam ticket, bug reports, ownership, achievements, stats, events, presence, patch notices, items, and the dedicated server.

Download a release zip and copy the `BlaziumGames` folder to `Plugins/BlaziumGames`, then enable Blazium Games. Windows and Linux are the supported targets.

The plugin loads `blazium_api64.dll` on Windows and `libblazium_api.so` on Linux from beside the game executable. It talks to the Blazium launcher on the player's machine. This plugin does not call `api.blazium.online`. macOS has no player build of the library.

Put `game.blaze` beside the game executable. This plugin does not read or write that file.

```json
{
  "game_uid": "550e8400-e29b-41d4-a716-446655440000"
}
```

`game_uid` is the project id on blazium.games. No other keys are allowed.

`UBlaziumSubsystem` ticks `RunCallbacks` while a client or dedicated-server session is open. Do not also call it every frame yourself. Blueprint nodes are in the Blazium category. Do not call client init and dedicated-server init in the same process.

`ReportBug` sends the in-game report. An empty build id and an empty screenshot path omit those parts. The screenshot path check lives in the engine host. This plugin does not proxy a Discord activity.

A push to `master` publishes `BlaziumGames-<version>.zip`. A `templates-*` tag is the engine host, not this library.

- Docs: [docs.blazium.games](https://docs.blazium.games) and the map [llms.txt](https://docs.blazium.games/llms.txt)
- Support: [blazium-games/support](https://github.com/blazium-games/support/issues). Status: [status.blazium.games](https://status.blazium.games)
- Guide: [Engines](https://docs.blazium.games/docs/engines)

## License

Licensed under the MIT License — see [LICENSE](LICENSE).

## Client

```cpp
FString Error;
const EBlaziumInitResult Result = FBlaziumApi::Get().InitClient(Error);
if (Result != EBlaziumInitResult::OK)
{
    return static_cast<int32>(Result);
}
if (!FBlaziumApi::Get().IsLoggedOn())
{
    FBlaziumApi::Get().ShutdownClient();
    return 1;
}
FBlaziumApi::Get().RunClientCallbacks();
const FString Uid = FBlaziumApi::Get().GetUserUid();
const FString Name = FBlaziumApi::Get().GetDisplayName();
const FString Username = FBlaziumApi::Get().GetUsername();
const FString Game = FBlaziumApi::Get().GetGameUid();
FBlaziumApi::Get().ShutdownClient();
```

Lobby, ICE, Steam ticket, and bug report return the JSON string the library returns. The plugin copies that string into an `FString` before it returns.

```cpp
const FString Lobby = FBlaziumApi::Get().CreateLobby(TEXT("public"));
const FString Ticket = FBlaziumApi::Get().SignInWithSteamTicket(HexTicket);
const FString Bug = FBlaziumApi::Get().ReportBug(TEXT("player fell through the floor"), TEXT("1.0.0"), TEXT("Windows"), TEXT("x64"), FString(), TEXT("build-1"), FString());
```

## Dedicated server

```cpp
FString Error;
const EBlaziumInitResult Result = FBlaziumApi::Get().InitServer(27015, TEXT("1.0.0"), Error);
if (Result != EBlaziumInitResult::OK)
{
    return static_cast<int32>(Result);
}
FBlaziumApi::Get().ServerLogOn();
const FString ServerId = FBlaziumApi::Get().GetServerId();
const FString Progress = FBlaziumApi::Get().ServerProgressRead(TEXT("server-key"), TEXT("player-uid"));
FBlaziumApi::Get().ShutdownServer();
```
