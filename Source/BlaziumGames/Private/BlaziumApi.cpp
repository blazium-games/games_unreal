#include "BlaziumApi.h"

#if PLATFORM_WINDOWS
#define BLAZIUM_CALL __cdecl
#else
#define BLAZIUM_CALL
#endif

struct FBlaziumApi::FNative
{
	typedef int (BLAZIUM_CALL* InitFlatFn)(char* Err);
	typedef void (BLAZIUM_CALL* VoidFn)();
	typedef int (BLAZIUM_CALL* IntFn)();
	typedef const char* (BLAZIUM_CALL* CStrFn)();
	typedef int (BLAZIUM_CALL* ServerInitFn)(uint16 Port, const char* Version, char* Err);
	typedef const char* (BLAZIUM_CALL* CStrArgFn)(const char* Arg);
	typedef const char* (BLAZIUM_CALL* ReportBugFn)(const char* Message, const char* ClientVersion, const char* OsName, const char* Arch, const char* LogText);
	typedef const char* (BLAZIUM_CALL* ReportInGameBugFn)(const char* Message, const char* ClientVersion, const char* OsName, const char* Arch, const char* LogText, const char* BuildId, const char* ScreenshotPath);
	typedef const char* (BLAZIUM_CALL* TwoStrFn)(const char* A, const char* B);
	typedef const char* (BLAZIUM_CALL* SetStatFn)(const char* Slug, int64 Value);
	typedef const char* (BLAZIUM_CALL* ThreeStrFn)(const char* A, const char* B, const char* C);
	typedef const char* (BLAZIUM_CALL* TransferItemFn)(const char* ItemIds, int64 ItemdefId, int64 Count, const char* Direction, const char* RequestId);
	typedef const char* (BLAZIUM_CALL* SocketItemFn)(const char* ParentId, const char* ChildId, int64 Index, const char* RequestId);
	typedef const char* (BLAZIUM_CALL* ServerStatFn)(const char* Key, const char* User, const char* Slug, int64 Value);

	InitFlatFn BlaziumAPI_InitFlat = nullptr;
	VoidFn BlaziumAPI_Shutdown = nullptr;
	VoidFn BlaziumAPI_RunCallbacks = nullptr;
	IntFn BlaziumAPI_IBlaziumUser_BLoggedOn = nullptr;
	CStrFn BlaziumAPI_IBlaziumUser_GetUserUid = nullptr;
	CStrFn BlaziumAPI_IBlaziumUser_GetDisplayName = nullptr;
	CStrFn BlaziumAPI_IBlaziumUser_GetAvatarUrl = nullptr;
	CStrFn BlaziumAPI_IBlaziumUser_GetUsername = nullptr;
	CStrFn BlaziumAPI_IBlaziumUser_GetGameUid = nullptr;
	ServerInitFn BlaziumGameServer_InitFlat = nullptr;
	VoidFn BlaziumGameServer_Shutdown = nullptr;
	VoidFn BlaziumGameServer_RunCallbacks = nullptr;
	IntFn BlaziumGameServer_LogOn = nullptr;
	VoidFn BlaziumGameServer_LogOff = nullptr;
	IntFn BlaziumGameServer_BLoggedOn = nullptr;
	CStrFn BlaziumGameServer_GetServerId = nullptr;
	CStrArgFn BlaziumAPI_CreateLobby = nullptr;
	CStrArgFn BlaziumAPI_JoinLobby = nullptr;
	CStrArgFn BlaziumAPI_LobbyStatus = nullptr;
	CStrArgFn BlaziumAPI_FetchIce = nullptr;
	CStrFn BlaziumAPI_LobbySeat = nullptr;
	CStrFn BlaziumAPI_LeaveLobby = nullptr;
	CStrArgFn BlaziumAPI_SignInWithSteamTicket = nullptr;
	CStrFn BlaziumAPI_GetSteamId = nullptr;
	CStrFn BlaziumAPI_GetSteamExpires = nullptr;
	ReportBugFn BlaziumAPI_ReportBug = nullptr;
	ReportInGameBugFn BlaziumAPI_ReportInGameBug = nullptr;
	CStrFn BlaziumAPI_BugNotices = nullptr;
	CStrArgFn BlaziumAPI_AckBugNotices = nullptr;
	TwoStrFn BlaziumAPI_Ownership = nullptr;
	CStrArgFn BlaziumAPI_UnlockAchievement = nullptr;
	SetStatFn BlaziumAPI_SetStat = nullptr;
	CStrArgFn BlaziumAPI_RecordEvent = nullptr;
	TwoStrFn BlaziumAPI_RecordStandardEvents = nullptr;
	CStrFn BlaziumAPI_Progress = nullptr;
	CStrArgFn BlaziumAPI_SetPresence = nullptr;
	CStrFn BlaziumAPI_PatchNotices = nullptr;
	CStrArgFn BlaziumAPI_AckPatchNotices = nullptr;
	TransferItemFn BlaziumAPI_TransferItem = nullptr;
	SocketItemFn BlaziumAPI_SocketItem = nullptr;
	TwoStrFn BlaziumAPI_UnsocketItem = nullptr;
	ThreeStrFn BlaziumAPI_SetItemStats = nullptr;
	TwoStrFn BlaziumAPI_UseItem = nullptr;
	ThreeStrFn BlaziumGameServer_UnlockAchievement = nullptr;
	ServerStatFn BlaziumGameServer_SetStat = nullptr;
	ThreeStrFn BlaziumGameServer_RecordEvent = nullptr;
	TwoStrFn BlaziumAPI_ServerProgressRead = nullptr;
};

namespace
{
class FUtf8Arg
{
public:
	explicit FUtf8Arg(const FString& Value)
		: Converted(*Value)
	{
	}

	const char* Get() const
	{
		return reinterpret_cast<const char*>(Converted.Get());
	}

private:
	FTCHARToUTF8 Converted;
};

FString CopyUtf8(const char* Text)
{
	if (Text == nullptr || Text[0] == '\0')
	{
		return FString();
	}
	return FString(UTF8_TO_TCHAR(Text));
}

template <typename T>
bool BindExport(void* Dll, const TCHAR* Name, T& Out)
{
	Out = reinterpret_cast<T>(FPlatformProcess::GetDllExport(Dll, Name));
	return Out != nullptr;
}
}

FBlaziumApi& FBlaziumApi::Get()
{
	static FBlaziumApi Instance;
	return Instance;
}

bool FBlaziumApi::EnsureLoaded()
{
	if (Dll != nullptr && Native != nullptr)
	{
		return true;
	}
#if PLATFORM_WINDOWS
	const TCHAR* Names[] = {TEXT("blazium_api64.dll"), TEXT("blazium_api64")};
#else
	const TCHAR* Names[] = {TEXT("libblazium_api.so"), TEXT("blazium_api")};
#endif
	for (const TCHAR* Name : Names)
	{
		Dll = FPlatformProcess::GetDllHandle(Name);
		if (Dll != nullptr)
		{
			break;
		}
	}
	if (Dll == nullptr)
	{
		return false;
	}
	Native = new FNative();
	const bool Bound =
		BindExport(Dll, TEXT("BlaziumAPI_InitFlat"), Native->BlaziumAPI_InitFlat) &&
		BindExport(Dll, TEXT("BlaziumAPI_Shutdown"), Native->BlaziumAPI_Shutdown) &&
		BindExport(Dll, TEXT("BlaziumAPI_RunCallbacks"), Native->BlaziumAPI_RunCallbacks) &&
		BindExport(Dll, TEXT("BlaziumAPI_IBlaziumUser_BLoggedOn"), Native->BlaziumAPI_IBlaziumUser_BLoggedOn) &&
		BindExport(Dll, TEXT("BlaziumAPI_IBlaziumUser_GetUserUid"), Native->BlaziumAPI_IBlaziumUser_GetUserUid) &&
		BindExport(Dll, TEXT("BlaziumAPI_IBlaziumUser_GetDisplayName"), Native->BlaziumAPI_IBlaziumUser_GetDisplayName) &&
		BindExport(Dll, TEXT("BlaziumAPI_IBlaziumUser_GetAvatarUrl"), Native->BlaziumAPI_IBlaziumUser_GetAvatarUrl) &&
		BindExport(Dll, TEXT("BlaziumAPI_IBlaziumUser_GetUsername"), Native->BlaziumAPI_IBlaziumUser_GetUsername) &&
		BindExport(Dll, TEXT("BlaziumAPI_IBlaziumUser_GetGameUid"), Native->BlaziumAPI_IBlaziumUser_GetGameUid) &&
		BindExport(Dll, TEXT("BlaziumGameServer_InitFlat"), Native->BlaziumGameServer_InitFlat) &&
		BindExport(Dll, TEXT("BlaziumGameServer_Shutdown"), Native->BlaziumGameServer_Shutdown) &&
		BindExport(Dll, TEXT("BlaziumGameServer_RunCallbacks"), Native->BlaziumGameServer_RunCallbacks) &&
		BindExport(Dll, TEXT("BlaziumGameServer_LogOn"), Native->BlaziumGameServer_LogOn) &&
		BindExport(Dll, TEXT("BlaziumGameServer_LogOff"), Native->BlaziumGameServer_LogOff) &&
		BindExport(Dll, TEXT("BlaziumGameServer_BLoggedOn"), Native->BlaziumGameServer_BLoggedOn) &&
		BindExport(Dll, TEXT("BlaziumGameServer_GetServerId"), Native->BlaziumGameServer_GetServerId) &&
		BindExport(Dll, TEXT("BlaziumAPI_CreateLobby"), Native->BlaziumAPI_CreateLobby) &&
		BindExport(Dll, TEXT("BlaziumAPI_JoinLobby"), Native->BlaziumAPI_JoinLobby) &&
		BindExport(Dll, TEXT("BlaziumAPI_LobbyStatus"), Native->BlaziumAPI_LobbyStatus) &&
		BindExport(Dll, TEXT("BlaziumAPI_FetchIce"), Native->BlaziumAPI_FetchIce) &&
		BindExport(Dll, TEXT("BlaziumAPI_LobbySeat"), Native->BlaziumAPI_LobbySeat) &&
		BindExport(Dll, TEXT("BlaziumAPI_LeaveLobby"), Native->BlaziumAPI_LeaveLobby) &&
		BindExport(Dll, TEXT("BlaziumAPI_SignInWithSteamTicket"), Native->BlaziumAPI_SignInWithSteamTicket) &&
		BindExport(Dll, TEXT("BlaziumAPI_GetSteamId"), Native->BlaziumAPI_GetSteamId) &&
		BindExport(Dll, TEXT("BlaziumAPI_GetSteamExpires"), Native->BlaziumAPI_GetSteamExpires) &&
		BindExport(Dll, TEXT("BlaziumAPI_ReportBug"), Native->BlaziumAPI_ReportBug) &&
		BindExport(Dll, TEXT("BlaziumAPI_ReportInGameBug"), Native->BlaziumAPI_ReportInGameBug) &&
		BindExport(Dll, TEXT("BlaziumAPI_BugNotices"), Native->BlaziumAPI_BugNotices) &&
		BindExport(Dll, TEXT("BlaziumAPI_AckBugNotices"), Native->BlaziumAPI_AckBugNotices) &&
		BindExport(Dll, TEXT("BlaziumAPI_Ownership"), Native->BlaziumAPI_Ownership) &&
		BindExport(Dll, TEXT("BlaziumAPI_UnlockAchievement"), Native->BlaziumAPI_UnlockAchievement) &&
		BindExport(Dll, TEXT("BlaziumAPI_SetStat"), Native->BlaziumAPI_SetStat) &&
		BindExport(Dll, TEXT("BlaziumAPI_RecordEvent"), Native->BlaziumAPI_RecordEvent) &&
		BindExport(Dll, TEXT("BlaziumAPI_RecordStandardEvents"), Native->BlaziumAPI_RecordStandardEvents) &&
		BindExport(Dll, TEXT("BlaziumAPI_Progress"), Native->BlaziumAPI_Progress) &&
		BindExport(Dll, TEXT("BlaziumAPI_SetPresence"), Native->BlaziumAPI_SetPresence) &&
		BindExport(Dll, TEXT("BlaziumAPI_PatchNotices"), Native->BlaziumAPI_PatchNotices) &&
		BindExport(Dll, TEXT("BlaziumAPI_AckPatchNotices"), Native->BlaziumAPI_AckPatchNotices) &&
		BindExport(Dll, TEXT("BlaziumAPI_TransferItem"), Native->BlaziumAPI_TransferItem) &&
		BindExport(Dll, TEXT("BlaziumAPI_SocketItem"), Native->BlaziumAPI_SocketItem) &&
		BindExport(Dll, TEXT("BlaziumAPI_UnsocketItem"), Native->BlaziumAPI_UnsocketItem) &&
		BindExport(Dll, TEXT("BlaziumAPI_SetItemStats"), Native->BlaziumAPI_SetItemStats) &&
		BindExport(Dll, TEXT("BlaziumAPI_UseItem"), Native->BlaziumAPI_UseItem) &&
		BindExport(Dll, TEXT("BlaziumGameServer_UnlockAchievement"), Native->BlaziumGameServer_UnlockAchievement) &&
		BindExport(Dll, TEXT("BlaziumGameServer_SetStat"), Native->BlaziumGameServer_SetStat) &&
		BindExport(Dll, TEXT("BlaziumGameServer_RecordEvent"), Native->BlaziumGameServer_RecordEvent) &&
		BindExport(Dll, TEXT("BlaziumAPI_ServerProgressRead"), Native->BlaziumAPI_ServerProgressRead);
	if (!Bound)
	{
		Release();
		return false;
	}
	return true;
}

void FBlaziumApi::Release()
{
	if (Mode == EMode::Client && Native != nullptr && Native->BlaziumAPI_Shutdown != nullptr)
	{
		Native->BlaziumAPI_Shutdown();
	}
	else if (Mode == EMode::Server && Native != nullptr && Native->BlaziumGameServer_Shutdown != nullptr)
	{
		Native->BlaziumGameServer_Shutdown();
	}
	Mode = EMode::None;
	if (Dll != nullptr)
	{
		FPlatformProcess::FreeDllHandle(Dll);
		Dll = nullptr;
	}
	delete Native;
	Native = nullptr;
}

EBlaziumInitResult FBlaziumApi::InitClient(FString& OutError)
{
	if (Mode == EMode::Server)
	{
		OutError = TEXT("Do not call client init and dedicated-server init in the same process.");
		return EBlaziumInitResult::FailedGeneric;
	}
	if (!EnsureLoaded())
	{
		OutError = TEXT("blazium_api64.dll or libblazium_api.so is missing");
		return EBlaziumInitResult::FailedGeneric;
	}
	char Err[1024] = {};
	const int Result = Native->BlaziumAPI_InitFlat(Err);
	OutError = CopyUtf8(Err);
	if (Result == static_cast<int>(EBlaziumInitResult::OK))
	{
		Mode = EMode::Client;
	}
	if (Result < 0 || Result > static_cast<int>(EBlaziumInitResult::NotOwned))
	{
		return EBlaziumInitResult::FailedGeneric;
	}
	return static_cast<EBlaziumInitResult>(Result);
}

void FBlaziumApi::ShutdownClient()
{
	if (Mode != EMode::Client || Native == nullptr)
	{
		return;
	}
	Native->BlaziumAPI_Shutdown();
	Mode = EMode::None;
}

void FBlaziumApi::RunClientCallbacks()
{
	if (Mode != EMode::Client || Native == nullptr)
	{
		return;
	}
	Native->BlaziumAPI_RunCallbacks();
}

EBlaziumInitResult FBlaziumApi::InitServer(int32 GamePort, const FString& Version, FString& OutError)
{
	if (Mode == EMode::Client)
	{
		OutError = TEXT("Do not call client init and dedicated-server init in the same process.");
		return EBlaziumInitResult::FailedGeneric;
	}
	if (GamePort < 0 || GamePort > 65535)
	{
		OutError = TEXT("game port is out of range");
		return EBlaziumInitResult::FailedGeneric;
	}
	if (!EnsureLoaded())
	{
		OutError = TEXT("blazium_api64.dll or libblazium_api.so is missing");
		return EBlaziumInitResult::FailedGeneric;
	}
	const FUtf8Arg VersionUtf8(Version);
	char Err[1024] = {};
	const int Result = Native->BlaziumGameServer_InitFlat(static_cast<uint16>(GamePort), VersionUtf8.Get(), Err);
	OutError = CopyUtf8(Err);
	if (Result == static_cast<int>(EBlaziumInitResult::OK))
	{
		Mode = EMode::Server;
	}
	if (Result < 0 || Result > static_cast<int>(EBlaziumInitResult::NotOwned))
	{
		return EBlaziumInitResult::FailedGeneric;
	}
	return static_cast<EBlaziumInitResult>(Result);
}

void FBlaziumApi::ShutdownServer()
{
	if (Mode != EMode::Server || Native == nullptr)
	{
		return;
	}
	Native->BlaziumGameServer_Shutdown();
	Mode = EMode::None;
}

void FBlaziumApi::RunServerCallbacks()
{
	if (Mode != EMode::Server || Native == nullptr)
	{
		return;
	}
	Native->BlaziumGameServer_RunCallbacks();
}

bool FBlaziumApi::ServerLogOn()
{
	if (Native == nullptr)
	{
		return false;
	}
	return Native->BlaziumGameServer_LogOn() != 0;
}

void FBlaziumApi::ServerLogOff()
{
	if (Native == nullptr)
	{
		return;
	}
	Native->BlaziumGameServer_LogOff();
}

bool FBlaziumApi::IsServerLoggedOn()
{
	if (Native == nullptr)
	{
		return false;
	}
	return Native->BlaziumGameServer_BLoggedOn() != 0;
}

FString FBlaziumApi::GetServerId()
{
	if (Native == nullptr)
	{
		return FString();
	}
	return CopyUtf8(Native->BlaziumGameServer_GetServerId());
}

bool FBlaziumApi::IsClientSession() const
{
	return Mode == EMode::Client;
}

bool FBlaziumApi::IsServerSession() const
{
	return Mode == EMode::Server;
}

bool FBlaziumApi::IsLoggedOn()
{
	if (Native == nullptr)
	{
		return false;
	}
	return Native->BlaziumAPI_IBlaziumUser_BLoggedOn() != 0;
}

FString FBlaziumApi::GetUserUid()
{
	if (Native == nullptr)
	{
		return FString();
	}
	return CopyUtf8(Native->BlaziumAPI_IBlaziumUser_GetUserUid());
}

FString FBlaziumApi::GetDisplayName()
{
	if (Native == nullptr)
	{
		return FString();
	}
	return CopyUtf8(Native->BlaziumAPI_IBlaziumUser_GetDisplayName());
}

FString FBlaziumApi::GetAvatarUrl()
{
	if (Native == nullptr)
	{
		return FString();
	}
	return CopyUtf8(Native->BlaziumAPI_IBlaziumUser_GetAvatarUrl());
}

FString FBlaziumApi::GetUsername()
{
	if (Native == nullptr)
	{
		return FString();
	}
	return CopyUtf8(Native->BlaziumAPI_IBlaziumUser_GetUsername());
}

FString FBlaziumApi::GetGameUid()
{
	if (Native == nullptr)
	{
		return FString();
	}
	return CopyUtf8(Native->BlaziumAPI_IBlaziumUser_GetGameUid());
}

FString FBlaziumApi::SignInWithSteamTicket(const FString& Ticket)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg TicketUtf8(Ticket);
	return CopyUtf8(Native->BlaziumAPI_SignInWithSteamTicket(TicketUtf8.Get()));
}

FString FBlaziumApi::GetSteamId()
{
	if (Native == nullptr)
	{
		return FString();
	}
	return CopyUtf8(Native->BlaziumAPI_GetSteamId());
}

FString FBlaziumApi::GetSteamExpires()
{
	if (Native == nullptr)
	{
		return FString();
	}
	return CopyUtf8(Native->BlaziumAPI_GetSteamExpires());
}

FString FBlaziumApi::ReportBug(const FString& Message, const FString& ClientVersion, const FString& OsName, const FString& Arch, const FString& LogText, const FString& BuildId, const FString& ScreenshotPath)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg MessageUtf8(Message);
	const FUtf8Arg VersionUtf8(ClientVersion);
	const FUtf8Arg OsUtf8(OsName);
	const FUtf8Arg ArchUtf8(Arch);
	const FUtf8Arg LogUtf8(LogText);
	const FUtf8Arg BuildUtf8(BuildId);
	const FUtf8Arg ShotUtf8(ScreenshotPath);
	return CopyUtf8(Native->BlaziumAPI_ReportInGameBug(MessageUtf8.Get(), VersionUtf8.Get(), OsUtf8.Get(), ArchUtf8.Get(), LogUtf8.Get(), BuildUtf8.Get(), ShotUtf8.Get()));
}

FString FBlaziumApi::BugNotices()
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	return CopyUtf8(Native->BlaziumAPI_BugNotices());
}

FString FBlaziumApi::AckBugNotices(const FString& UidsJson)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg UidsUtf8(UidsJson);
	return CopyUtf8(Native->BlaziumAPI_AckBugNotices(UidsUtf8.Get()));
}

FString FBlaziumApi::Ownership(const FString& Edition, const FString& ListingUid)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg EditionUtf8(Edition);
	const FUtf8Arg ListingUtf8(ListingUid);
	return CopyUtf8(Native->BlaziumAPI_Ownership(EditionUtf8.Get(), ListingUtf8.Get()));
}

FString FBlaziumApi::UnlockAchievement(const FString& Slug)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg SlugUtf8(Slug);
	return CopyUtf8(Native->BlaziumAPI_UnlockAchievement(SlugUtf8.Get()));
}

FString FBlaziumApi::SetStat(const FString& Slug, int64 Value)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg SlugUtf8(Slug);
	return CopyUtf8(Native->BlaziumAPI_SetStat(SlugUtf8.Get(), Value));
}

FString FBlaziumApi::RecordEvent(const FString& Name)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg NameUtf8(Name);
	return CopyUtf8(Native->BlaziumAPI_RecordEvent(NameUtf8.Get()));
}

FString FBlaziumApi::RecordStandardEvents(const FString& BuildId, const FString& DeviceUid)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg BuildUtf8(BuildId);
	const FUtf8Arg DeviceUtf8(DeviceUid);
	return CopyUtf8(Native->BlaziumAPI_RecordStandardEvents(BuildUtf8.Get(), DeviceUtf8.Get()));
}

FString FBlaziumApi::Progress()
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	return CopyUtf8(Native->BlaziumAPI_Progress());
}

FString FBlaziumApi::SetPresence(const FString& Status)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg StatusUtf8(Status);
	return CopyUtf8(Native->BlaziumAPI_SetPresence(StatusUtf8.Get()));
}

FString FBlaziumApi::PatchNotices()
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	return CopyUtf8(Native->BlaziumAPI_PatchNotices());
}

FString FBlaziumApi::AckPatchNotices(const FString& UidsJson)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg UidsUtf8(UidsJson);
	return CopyUtf8(Native->BlaziumAPI_AckPatchNotices(UidsUtf8.Get()));
}

FString FBlaziumApi::TransferItem(const FString& ItemIdsJson, int64 ItemdefId, int64 Count, const FString& Direction, const FString& RequestId)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg IdsUtf8(ItemIdsJson);
	const FUtf8Arg DirectionUtf8(Direction);
	const FUtf8Arg RequestUtf8(RequestId);
	return CopyUtf8(Native->BlaziumAPI_TransferItem(IdsUtf8.Get(), ItemdefId, Count, DirectionUtf8.Get(), RequestUtf8.Get()));
}

FString FBlaziumApi::SocketItem(const FString& ParentId, const FString& ChildId, int64 Index, const FString& RequestId)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg ParentUtf8(ParentId);
	const FUtf8Arg ChildUtf8(ChildId);
	const FUtf8Arg RequestUtf8(RequestId);
	return CopyUtf8(Native->BlaziumAPI_SocketItem(ParentUtf8.Get(), ChildUtf8.Get(), Index, RequestUtf8.Get()));
}

FString FBlaziumApi::UnsocketItem(const FString& ChildId, const FString& RequestId)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg ChildUtf8(ChildId);
	const FUtf8Arg RequestUtf8(RequestId);
	return CopyUtf8(Native->BlaziumAPI_UnsocketItem(ChildUtf8.Get(), RequestUtf8.Get()));
}

FString FBlaziumApi::SetItemStats(const FString& ItemId, const FString& StatsJson, const FString& RequestId)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg ItemUtf8(ItemId);
	const FUtf8Arg StatsUtf8(StatsJson);
	const FUtf8Arg RequestUtf8(RequestId);
	return CopyUtf8(Native->BlaziumAPI_SetItemStats(ItemUtf8.Get(), StatsUtf8.Get(), RequestUtf8.Get()));
}

FString FBlaziumApi::UseItem(const FString& ItemId, const FString& RequestId)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg ItemUtf8(ItemId);
	const FUtf8Arg RequestUtf8(RequestId);
	return CopyUtf8(Native->BlaziumAPI_UseItem(ItemUtf8.Get(), RequestUtf8.Get()));
}

FString FBlaziumApi::ServerUnlockAchievement(const FString& ServerKey, const FString& UserUid, const FString& Slug)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg KeyUtf8(ServerKey);
	const FUtf8Arg UserUtf8(UserUid);
	const FUtf8Arg SlugUtf8(Slug);
	return CopyUtf8(Native->BlaziumGameServer_UnlockAchievement(KeyUtf8.Get(), UserUtf8.Get(), SlugUtf8.Get()));
}

FString FBlaziumApi::ServerSetStat(const FString& ServerKey, const FString& UserUid, const FString& Slug, int64 Value)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg KeyUtf8(ServerKey);
	const FUtf8Arg UserUtf8(UserUid);
	const FUtf8Arg SlugUtf8(Slug);
	return CopyUtf8(Native->BlaziumGameServer_SetStat(KeyUtf8.Get(), UserUtf8.Get(), SlugUtf8.Get(), Value));
}

FString FBlaziumApi::ServerRecordEvent(const FString& ServerKey, const FString& UserUid, const FString& Name)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg KeyUtf8(ServerKey);
	const FUtf8Arg UserUtf8(UserUid);
	const FUtf8Arg NameUtf8(Name);
	return CopyUtf8(Native->BlaziumGameServer_RecordEvent(KeyUtf8.Get(), UserUtf8.Get(), NameUtf8.Get()));
}

FString FBlaziumApi::ServerProgressRead(const FString& ServerKey, const FString& UserUid)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg KeyUtf8(ServerKey);
	const FUtf8Arg UserUtf8(UserUid);
	return CopyUtf8(Native->BlaziumAPI_ServerProgressRead(KeyUtf8.Get(), UserUtf8.Get()));
}

FString FBlaziumApi::CreateLobby(const FString& LobbyType)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg TypeUtf8(LobbyType);
	return CopyUtf8(Native->BlaziumAPI_CreateLobby(TypeUtf8.Get()));
}

FString FBlaziumApi::JoinLobby(const FString& LobbyId)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg IdUtf8(LobbyId);
	return CopyUtf8(Native->BlaziumAPI_JoinLobby(IdUtf8.Get()));
}

FString FBlaziumApi::LobbyStatus(const FString& LobbyId)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg IdUtf8(LobbyId);
	return CopyUtf8(Native->BlaziumAPI_LobbyStatus(IdUtf8.Get()));
}

FString FBlaziumApi::FetchIce(const FString& SessionId)
{
	if (!EnsureLoaded())
	{
		return FString();
	}
	const FUtf8Arg SessionUtf8(SessionId);
	return CopyUtf8(Native->BlaziumAPI_FetchIce(SessionUtf8.Get()));
}

FString FBlaziumApi::LobbySeat()
{
	if (Native == nullptr)
	{
		return FString();
	}
	return CopyUtf8(Native->BlaziumAPI_LobbySeat());
}

FString FBlaziumApi::LeaveLobby()
{
	if (Native == nullptr)
	{
		return FString();
	}
	return CopyUtf8(Native->BlaziumAPI_LeaveLobby());
}
