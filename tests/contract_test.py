from pathlib import Path
import re
import sys

# Keep this list aligned with games_sdk/public/blazium/blazium_api_flat.h (SDK 2.2.0).
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
    "BlaziumAPI_RequestToken",
    "BlaziumAPI_ForgetTokens",
    "BlaziumAPI_StartLogin",
    "BlaziumAPI_LoginState",
    "BlaziumAPI_CancelLogin",
    "BlaziumAPI_Logout",
    "BlaziumAPI_ExportSession",
    "BlaziumAPI_RestoreSession",
    "BlaziumGameServer_VerifyPlayer",
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
    # The callback setters are for hosts that cannot poll; the plugin polls with RunCallbacks.
    found = [name for name in found if name not in ("BlaziumAPI_SetConnectionCallback", "BlaziumAPI_SetUserChangedCallback")]
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

sapi_h = Path("Source/BlaziumGames/Public/BlaziumSapi.h").read_text(encoding="utf-8")
sapi_cpp = Path("Source/BlaziumGames/Private/BlaziumSapi.cpp").read_text(encoding="utf-8")
for needle in ("UBlueprintAsyncActionBase", "CallLogic", "ValidLogic", "MaxBodyBytes"):
    if needle not in sapi_h:
        print("Scripted API client is missing " + needle)
        sys.exit(1)
if 'RequestToken(TEXT("sapi"))' not in sapi_cpp:
    print("Scripted API client must ask the host for a sapi token")
    sys.exit(1)
for banned in ("steam_token", "GetSteamId", "GetSteamExpires", "SignInWithSteamTicket"):
    if banned in sapi_cpp:
        print("Scripted API client must not read login material: " + banned)
        sys.exit(1)
if "Idempotency-Key" not in sapi_cpp or "ForgetTokens" not in sapi_cpp:
    print("Scripted API client must keep one Idempotency-Key and refresh the token once after a 401")
    sys.exit(1)
if '"HTTP"' not in build:
    print("the HTTP module is not a dependency")
    sys.exit(1)
if "Token" in Path("Source/BlaziumGames/Public/BlaziumBlueprintLibrary.h").read_text(encoding="utf-8"):
    print("a token must not be a Blueprint value")
    sys.exit(1)

player_h = Path("Source/BlaziumGames/Public/BlaziumPlayerSession.h").read_text(encoding="utf-8")
player_cpp = Path("Source/BlaziumGames/Private/BlaziumPlayerSession.cpp").read_text(encoding="utf-8")
for needle in ("UBlaziumPlayerSession", "StartSignIn", "GetLoginUrl", "GetUserCode", "RestoreSaved", "FBlaziumPlayerVerifier", "FBlaziumSessionRules"):
    if needle not in player_h:
        print("player session is missing " + needle)
        sys.exit(1)
for needle in ("/api/v1/session", "/public/token/refresh", "/private/logout", "/tokens", "/server/verify-player", "X-Server-Key", "RefreshDue"):
    if needle not in player_cpp:
        print("player session does not call " + needle)
        sys.exit(1)
if "UFUNCTION" in player_h and "GetToken" in player_h.split("void GetToken")[0].split("UFUNCTION")[-1]:
    print("a token must not be a Blueprint value")
    sys.exit(1)
if "ExportSession" in "".join(line for line in player_h.splitlines() if "UFUNCTION" in line):
    print("the session must not be a Blueprint value")
    sys.exit(1)
if "UE_LOG" in player_cpp:
    print("the player session must not log; it handles secrets")
    sys.exit(1)
if "BlaziumPlayerSession" not in sapi_cpp or "SendWithToken" not in sapi_cpp:
    print("the Scripted API client does not use the player session")
    sys.exit(1)
if '"Json"' not in build:
    print("the Json module is not a dependency")
    sys.exit(1)
print("unreal contract ok")
