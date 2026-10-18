# Local testing

`games_unreal` is an Unreal plugin. CI checks that every export in `blazium_api_flat.h` is resolved by name. It does not start the Unreal editor.

```text
python tests/contract_test.py
```

To try it in the editor, copy this directory to `Plugins/BlaziumGames`, put `blazium_api64.dll` or `libblazium_api.so` next to the game executable, and put `game.blaze` there too. The launcher has to be running and a player has to be logged in before client init succeeds.

The plugin covers init, callbacks, the user, lobbies, a Steam ticket, bug reports, ownership, achievements, stats, events, presence, patch notices, items, and the dedicated server. The README samples show a lobby call, an in-game bug, and a server progress read. `ReportBug` sends the in-game report.
