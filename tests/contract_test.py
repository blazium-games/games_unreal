from pathlib import Path
import re
import sys

# Keep this list aligned with games_sdk/public/blazium/blazium_api_flat.h (SDK 2.0.0).
SYMBOLS = [
    "BlaziumAPI_InitFlat",
    "BlaziumAPI_Shutdown",
    "BlaziumAPI_RunCallbacks",
    "BlaziumAPI_IBlaziumUser_BLoggedOn",
    "BlaziumAPI_IBlaziumUser_GetUserUid",
    "BlaziumAPI_IBlaziumUser_GetDisplayName",
    "BlaziumAPI_IBlaziumUser_GetAvatarUrl",
    "BlaziumAPI_IBlaziumUser_GetUsername",
    "BlaziumAPI_IBlaziumUser_GetGameUid",
    "BlaziumGameServer_InitFlat",
    "BlaziumGameServer_Shutdown",
    "BlaziumGameServer_RunCallbacks",
    "BlaziumGameServer_LogOn",
    "BlaziumGameServer_LogOff",
    "BlaziumGameServer_BLoggedOn",
    "BlaziumGameServer_GetServerId",
    "BlaziumAPI_CreateLobby",
    "BlaziumAPI_JoinLobby",
    "BlaziumAPI_LobbyStatus",
    "BlaziumAPI_FetchIce",
    "BlaziumAPI_LobbySeat",
    "BlaziumAPI_LeaveLobby",
    "BlaziumAPI_SignInWithSteamTicket",
    "BlaziumAPI_GetSteamId",
    "BlaziumAPI_GetSteamExpires",
    "BlaziumAPI_ReportBug",
    "BlaziumAPI_ReportInGameBug",
    "BlaziumAPI_BugNotices",
    "BlaziumAPI_AckBugNotices",
    "BlaziumAPI_Ownership",
    "BlaziumAPI_UnlockAchievement",
    "BlaziumAPI_SetStat",
    "BlaziumAPI_RecordEvent",
    "BlaziumAPI_RecordStandardEvents",
    "BlaziumAPI_Progress",
    "BlaziumAPI_SetPresence",
    "BlaziumAPI_PatchNotices",
    "BlaziumAPI_AckPatchNotices",
    "BlaziumAPI_TransferItem",
    "BlaziumAPI_SocketItem",
    "BlaziumAPI_UnsocketItem",
    "BlaziumAPI_SetItemStats",
    "BlaziumAPI_UseItem",
    "BlaziumGameServer_UnlockAchievement",
    "BlaziumGameServer_SetStat",
    "BlaziumGameServer_RecordEvent",
    "BlaziumAPI_ServerProgressRead",
]

root = Path("Source")
files = list(root.rglob("*.h")) + list(root.rglob("*.cpp"))
if not files:
    print("no plugin sources")
    sys.exit(1)
text = "\n".join(path.read_text(encoding="utf-8") for path in files)
missing = [name for name in SYMBOLS if name not in text]
if missing:
    print("missing exports: " + ", ".join(missing))
    sys.exit(1)

header = Path("../games_sdk/public/blazium/blazium_api_flat.h")
if header.is_file():
    found = re.findall(r"S_CALLTYPE\s+([A-Za-z0-9_]+)\s*\(", header.read_text(encoding="utf-8"))
    if found != SYMBOLS:
        print("flat header exports differ from the contract list")
        print("header: " + ", ".join(found))
        sys.exit(1)

api = Path("Source/BlaziumGames/Private/BlaziumApi.cpp").read_text(encoding="utf-8")
if "GetDllHandle(Name)" in api or 'TEXT("blazium_api64")' in api:
    print("native library must load from the plugin binaries directory")
    sys.exit(1)
if "FPaths::Combine" not in api or "ProjectPluginsDir" not in api:
    print("native library path is not pinned to the plugin")
    sys.exit(1)
if "IPluginManager::Get().FindPlugin" not in api:
    print("loader ignores an engine-installed plugin")
    sys.exit(1)
if "libblazium_api.dylib" in api:
    print("macOS player library is not shipped")
    sys.exit(1)
if "is missing (%s)" not in api:
    print("error does not include the path that was checked")
    sys.exit(1)
build = Path("Source/BlaziumGames/BlaziumGames.Build.cs").read_text(encoding="utf-8")
if "RuntimeDependencies.Add" not in build or "blazium_api64.dll" not in build or "libblazium_api.so" not in build:
    print("packaging does not stage the native library")
    sys.exit(1)
if "libblazium_api.dylib" in build:
    print("packaging stages a macOS library that is not shipped")
    sys.exit(1)
subsystem = Path("Source/BlaziumGames/Private/BlaziumSubsystem.cpp").read_text(encoding="utf-8")
if "GCallbackOwner" not in subsystem:
    print("native callbacks must run once for the process")
    sys.exit(1)

print("unreal contract ok")
