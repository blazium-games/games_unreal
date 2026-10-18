#include "BlaziumBlueprintLibrary.h"

#include "BlaziumApi.h"

EBlaziumInitResult UBlaziumBlueprintLibrary::Init(FString& OutError)
{
	return FBlaziumApi::Get().InitClient(OutError);
}

void UBlaziumBlueprintLibrary::Shutdown()
{
	FBlaziumApi::Get().ShutdownClient();
}

bool UBlaziumBlueprintLibrary::IsLoggedOn()
{
	return FBlaziumApi::Get().IsLoggedOn();
}

FString UBlaziumBlueprintLibrary::GetUserUid()
{
	return FBlaziumApi::Get().GetUserUid();
}

FString UBlaziumBlueprintLibrary::GetDisplayName()
{
	return FBlaziumApi::Get().GetDisplayName();
}

FString UBlaziumBlueprintLibrary::GetAvatarUrl()
{
	return FBlaziumApi::Get().GetAvatarUrl();
}

FString UBlaziumBlueprintLibrary::GetUsername()
{
	return FBlaziumApi::Get().GetUsername();
}

FString UBlaziumBlueprintLibrary::GetGameUid()
{
	return FBlaziumApi::Get().GetGameUid();
}

FString UBlaziumBlueprintLibrary::SignInWithSteamTicket(const FString& Ticket)
{
	return FBlaziumApi::Get().SignInWithSteamTicket(Ticket);
}

FString UBlaziumBlueprintLibrary::GetSteamId()
{
	return FBlaziumApi::Get().GetSteamId();
}

FString UBlaziumBlueprintLibrary::GetSteamExpires()
{
	return FBlaziumApi::Get().GetSteamExpires();
}

FString UBlaziumBlueprintLibrary::ReportBug(const FString& Message, const FString& ClientVersion, const FString& OsName, const FString& Arch, const FString& LogText, const FString& BuildId, const FString& ScreenshotPath)
{
	return FBlaziumApi::Get().ReportBug(Message, ClientVersion, OsName, Arch, LogText, BuildId, ScreenshotPath);
}

FString UBlaziumBlueprintLibrary::BugNotices()
{
	return FBlaziumApi::Get().BugNotices();
}

FString UBlaziumBlueprintLibrary::AckBugNotices(const FString& UidsJson)
{
	return FBlaziumApi::Get().AckBugNotices(UidsJson);
}

FString UBlaziumBlueprintLibrary::Ownership(const FString& Edition, const FString& ListingUid)
{
	return FBlaziumApi::Get().Ownership(Edition, ListingUid);
}

FString UBlaziumBlueprintLibrary::UnlockAchievement(const FString& Slug)
{
	return FBlaziumApi::Get().UnlockAchievement(Slug);
}

FString UBlaziumBlueprintLibrary::SetStat(const FString& Slug, int64 Value)
{
	return FBlaziumApi::Get().SetStat(Slug, Value);
}

FString UBlaziumBlueprintLibrary::RecordEvent(const FString& Name)
{
	return FBlaziumApi::Get().RecordEvent(Name);
}

FString UBlaziumBlueprintLibrary::RecordStandardEvents(const FString& BuildId, const FString& DeviceUid)
{
	return FBlaziumApi::Get().RecordStandardEvents(BuildId, DeviceUid);
}

FString UBlaziumBlueprintLibrary::Progress()
{
	return FBlaziumApi::Get().Progress();
}

FString UBlaziumBlueprintLibrary::SetPresence(const FString& Status)
{
	return FBlaziumApi::Get().SetPresence(Status);
}

FString UBlaziumBlueprintLibrary::PatchNotices()
{
	return FBlaziumApi::Get().PatchNotices();
}

FString UBlaziumBlueprintLibrary::AckPatchNotices(const FString& UidsJson)
{
	return FBlaziumApi::Get().AckPatchNotices(UidsJson);
}

FString UBlaziumBlueprintLibrary::TransferItem(const FString& ItemIdsJson, int64 ItemdefId, int64 Count, const FString& Direction, const FString& RequestId)
{
	return FBlaziumApi::Get().TransferItem(ItemIdsJson, ItemdefId, Count, Direction, RequestId);
}

FString UBlaziumBlueprintLibrary::SocketItem(const FString& ParentId, const FString& ChildId, int64 Index, const FString& RequestId)
{
	return FBlaziumApi::Get().SocketItem(ParentId, ChildId, Index, RequestId);
}

FString UBlaziumBlueprintLibrary::UnsocketItem(const FString& ChildId, const FString& RequestId)
{
	return FBlaziumApi::Get().UnsocketItem(ChildId, RequestId);
}

FString UBlaziumBlueprintLibrary::SetItemStats(const FString& ItemId, const FString& StatsJson, const FString& RequestId)
{
	return FBlaziumApi::Get().SetItemStats(ItemId, StatsJson, RequestId);
}

FString UBlaziumBlueprintLibrary::UseItem(const FString& ItemId, const FString& RequestId)
{
	return FBlaziumApi::Get().UseItem(ItemId, RequestId);
}

FString UBlaziumBlueprintLibrary::ServerUnlockAchievement(const FString& ServerKey, const FString& UserUid, const FString& Slug)
{
	return FBlaziumApi::Get().ServerUnlockAchievement(ServerKey, UserUid, Slug);
}

FString UBlaziumBlueprintLibrary::ServerSetStat(const FString& ServerKey, const FString& UserUid, const FString& Slug, int64 Value)
{
	return FBlaziumApi::Get().ServerSetStat(ServerKey, UserUid, Slug, Value);
}

FString UBlaziumBlueprintLibrary::ServerRecordEvent(const FString& ServerKey, const FString& UserUid, const FString& Name)
{
	return FBlaziumApi::Get().ServerRecordEvent(ServerKey, UserUid, Name);
}

FString UBlaziumBlueprintLibrary::ServerProgressRead(const FString& ServerKey, const FString& UserUid)
{
	return FBlaziumApi::Get().ServerProgressRead(ServerKey, UserUid);
}

FString UBlaziumBlueprintLibrary::CreateLobby(const FString& LobbyType)
{
	return FBlaziumApi::Get().CreateLobby(LobbyType);
}

FString UBlaziumBlueprintLibrary::JoinLobby(const FString& LobbyId)
{
	return FBlaziumApi::Get().JoinLobby(LobbyId);
}

FString UBlaziumBlueprintLibrary::LobbyStatus(const FString& LobbyId)
{
	return FBlaziumApi::Get().LobbyStatus(LobbyId);
}

FString UBlaziumBlueprintLibrary::FetchIce(const FString& SessionId)
{
	return FBlaziumApi::Get().FetchIce(SessionId);
}

FString UBlaziumBlueprintLibrary::LobbySeat()
{
	return FBlaziumApi::Get().LobbySeat();
}

FString UBlaziumBlueprintLibrary::LeaveLobby()
{
	return FBlaziumApi::Get().LeaveLobby();
}

EBlaziumInitResult UBlaziumBlueprintLibrary::ServerInit(int32 GamePort, const FString& Version, FString& OutError)
{
	return FBlaziumApi::Get().InitServer(GamePort, Version, OutError);
}

void UBlaziumBlueprintLibrary::ServerShutdown()
{
	FBlaziumApi::Get().ShutdownServer();
}

bool UBlaziumBlueprintLibrary::ServerLogOn()
{
	return FBlaziumApi::Get().ServerLogOn();
}

void UBlaziumBlueprintLibrary::ServerLogOff()
{
	FBlaziumApi::Get().ServerLogOff();
}

bool UBlaziumBlueprintLibrary::IsServerLoggedOn()
{
	return FBlaziumApi::Get().IsServerLoggedOn();
}

FString UBlaziumBlueprintLibrary::GetServerId()
{
	return FBlaziumApi::Get().GetServerId();
}
