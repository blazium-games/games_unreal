#pragma once

#include "CoreMinimal.h"
#include "BlaziumTypes.h"

class BLAZIUMGAMES_API FBlaziumApi
{
public:
	static FBlaziumApi& Get();

	EBlaziumInitResult InitClient(FString& OutError);
	void ShutdownClient();
	void RunClientCallbacks();

	EBlaziumInitResult InitServer(int32 GamePort, const FString& Version, FString& OutError);
	void ShutdownServer();
	void RunServerCallbacks();
	bool ServerLogOn();
	void ServerLogOff();
	bool IsServerLoggedOn();
	FString GetServerId();

	bool IsClientSession() const;
	bool IsServerSession() const;

	bool IsLoggedOn();
	FString GetUserUid();
	FString GetDisplayName();
	FString GetAvatarUrl();
	FString GetUsername();
	FString GetGameUid();
	FString SignInWithSteamTicket(const FString& Ticket);
	FString GetSteamId();
	FString GetSteamExpires();
	FString ReportBug(const FString& Message, const FString& ClientVersion, const FString& OsName, const FString& Arch, const FString& LogText, const FString& BuildId = FString(), const FString& ScreenshotPath = FString());
	FString BugNotices();
	FString AckBugNotices(const FString& UidsJson);
	FString Ownership(const FString& Edition, const FString& ListingUid);
	FString UnlockAchievement(const FString& Slug);
	FString SetStat(const FString& Slug, int64 Value);
	FString RecordEvent(const FString& Name);
	FString RecordStandardEvents(const FString& BuildId, const FString& DeviceUid);
	FString Progress();
	FString SetPresence(const FString& Status);
	FString PatchNotices();
	FString AckPatchNotices(const FString& UidsJson);
	FString TransferItem(const FString& ItemIdsJson, int64 ItemdefId, int64 Count, const FString& Direction, const FString& RequestId);
	FString SocketItem(const FString& ParentId, const FString& ChildId, int64 Index, const FString& RequestId);
	FString UnsocketItem(const FString& ChildId, const FString& RequestId);
	FString SetItemStats(const FString& ItemId, const FString& StatsJson, const FString& RequestId);
	FString UseItem(const FString& ItemId, const FString& RequestId);
	FString ServerUnlockAchievement(const FString& ServerKey, const FString& UserUid, const FString& Slug);
	FString ServerSetStat(const FString& ServerKey, const FString& UserUid, const FString& Slug, int64 Value);
	FString ServerRecordEvent(const FString& ServerKey, const FString& UserUid, const FString& Name);
	FString ServerProgressRead(const FString& ServerKey, const FString& UserUid);

	FString CreateLobby(const FString& LobbyType);
	FString JoinLobby(const FString& LobbyId);
	FString LobbyStatus(const FString& LobbyId);
	FString FetchIce(const FString& SessionId);
	FString LobbySeat();
	FString LeaveLobby();

	// Asks the host for a short-lived token for this game. Audience is "sapi" or "lobby". Returns the JSON reply
	// ({"ok":true,"token":...} or {"ok":false,"code":...}). Needs SDK 2.1.0 or newer; an older library returns sdk_too_old.
	// The token is a secret: never log it or save it.
	FString RequestToken(const FString& Audience);
	// Drops cached tokens, so the next RequestToken asks again (after a 401).
	void ForgetTokens();

	// Launcher-less sign-in through the native library (SDK 2.2.0 or newer). Each returns JSON:
	// {"ok":true,...} or {"ok":false,"code":...}. StartLogin returns at once; poll LoginState.
	// The session is a secret: never log it. Keep it only in the platform's protected storage.
	FString StartLogin();
	FString LoginState();
	FString CancelLogin();
	FString Logout();
	FString ExportSession();
	FString RestoreSession(const FString& Session);
	// A dedicated server proves who a player is with the key of the server and the player's "server" token.
	FString VerifyPlayer(const FString& ServerKey, const FString& PlayerToken);

	void Release();

private:
	FBlaziumApi() = default;
	~FBlaziumApi() = default;

	bool EnsureLoaded();
	struct FNative;
	FNative* Native = nullptr;
	void* Dll = nullptr;
	FString LibraryPathChecked;

	enum class EMode : uint8
	{
		None,
		Client,
		Server,
	};
	EMode Mode = EMode::None;
};
