#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlaziumTypes.h"
#include "BlaziumBlueprintLibrary.generated.h"

UCLASS()
class BLAZIUMGAMES_API UBlaziumBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static EBlaziumInitResult Init(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static void Shutdown();

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static bool IsLoggedOn();

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString GetUserUid();

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString GetDisplayName();

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString GetAvatarUrl();

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString GetUsername();

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString GetGameUid();

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString SignInWithSteamTicket(const FString& Ticket);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString GetSteamId();

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString GetSteamExpires();

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString ReportBug(const FString& Message, const FString& ClientVersion, const FString& OsName, const FString& Arch, const FString& LogText, const FString& BuildId, const FString& ScreenshotPath);
	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString BugNotices();

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString AckBugNotices(const FString& UidsJson);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString Ownership(const FString& Edition, const FString& ListingUid);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString UnlockAchievement(const FString& Slug);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString SetStat(const FString& Slug, int64 Value);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString RecordEvent(const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString RecordStandardEvents(const FString& BuildId, const FString& DeviceUid);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString Progress();

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString SetPresence(const FString& Status);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString PatchNotices();

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString AckPatchNotices(const FString& UidsJson);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString TransferItem(const FString& ItemIdsJson, int64 ItemdefId, int64 Count, const FString& Direction, const FString& RequestId);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString SocketItem(const FString& ParentId, const FString& ChildId, int64 Index, const FString& RequestId);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString UnsocketItem(const FString& ChildId, const FString& RequestId);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString SetItemStats(const FString& ItemId, const FString& StatsJson, const FString& RequestId);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString UseItem(const FString& ItemId, const FString& RequestId);

	UFUNCTION(BlueprintCallable, Category = "Blazium|Server")
	static FString ServerUnlockAchievement(const FString& ServerKey, const FString& UserUid, const FString& Slug);

	UFUNCTION(BlueprintCallable, Category = "Blazium|Server")
	static FString ServerSetStat(const FString& ServerKey, const FString& UserUid, const FString& Slug, int64 Value);

	UFUNCTION(BlueprintCallable, Category = "Blazium|Server")
	static FString ServerRecordEvent(const FString& ServerKey, const FString& UserUid, const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "Blazium|Server")
	static FString ServerProgressRead(const FString& ServerKey, const FString& UserUid);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString CreateLobby(const FString& LobbyType);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString JoinLobby(const FString& LobbyId);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString LobbyStatus(const FString& LobbyId);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString FetchIce(const FString& SessionId);

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString LobbySeat();

	UFUNCTION(BlueprintCallable, Category = "Blazium")
	static FString LeaveLobby();

	UFUNCTION(BlueprintCallable, Category = "Blazium|Server")
	static EBlaziumInitResult ServerInit(int32 GamePort, const FString& Version, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Blazium|Server")
	static void ServerShutdown();

	UFUNCTION(BlueprintCallable, Category = "Blazium|Server")
	static bool ServerLogOn();

	UFUNCTION(BlueprintCallable, Category = "Blazium|Server")
	static void ServerLogOff();

	UFUNCTION(BlueprintCallable, Category = "Blazium|Server")
	static bool IsServerLoggedOn();

	UFUNCTION(BlueprintCallable, Category = "Blazium|Server")
	static FString GetServerId();
};
