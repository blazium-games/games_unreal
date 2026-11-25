#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Interfaces/IHttpRequest.h"
#include "BlaziumSapi.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FBlaziumSapiDone, bool, bSuccess, int32, Status, const FString&, Body, const FString&, ErrorKey);

// Plain helpers for the Scripted API client. No engine state, so they are easy to test.
struct BLAZIUMGAMES_API FBlaziumSapiRules
{
	static constexpr int32 MaxBodyBytes = 64 * 1024;
	static constexpr int32 MaxRetries = 3;

	// Lowercase letters, digits, "_" and "-", 1 to 64 characters.
	static bool ValidLogic(const FString& Logic);
	// Empty when the method is not allowed. Otherwise the upper-case method.
	static FString NormalizeMethod(const FString& Method);
	static FString BuildUrl(const FString& BaseUrl, const FString& GameUid, const FString& Logic, const FString& Method, const FString& Body);
	// Seconds before the next try: the Retry-After header when set, else 0.5, 1, 2 ... capped at 8.
	static float RetryDelay(int32 Attempt, const FString& RetryAfter);
	// Reads "key": "value" of the error envelope. Empty when there is none.
	static FString ErrorKey(const FString& Body);
};

// Calls this game's Scripted API at https://sapi.blazium.online/v1/{game_uid}/{logic}.
// The call asks the host for a short-lived token for this game (RequestToken "sapi"); the login token
// is never read. A 429 or 503 is retried with the same Idempotency-Key, and a 401 asks for a fresh
// token once. Logic marked auth = anonymous can pass bAnonymous to skip the token.
UCLASS()
class BLAZIUMGAMES_API UBlaziumSapi : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Blazium|Scripted API")
	static UBlaziumSapi* CallLogic(UObject* WorldContextObject, const FString& Logic, const FString& Method, const FString& BodyJson, bool bAnonymous);

	UPROPERTY(BlueprintAssignable)
	FBlaziumSapiDone OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FBlaziumSapiDone OnFailure;

	// Defaults to https://sapi.blazium.online. Set before Activate for a local service.
	UPROPERTY()
	FString BaseUrl = TEXT("https://sapi.blazium.online");

	UPROPERTY()
	FString Channel = TEXT("stable");

	virtual void Activate() override;

private:
	void Send();
	void SendWithToken(const FString& GameUid, const FString& Token);
	void OnResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected);
	void Finish(bool bSuccess, int32 Status, const FString& Body, const FString& ErrorKey);

	FString Logic;
	FString Method;
	FString Body;
	FString IdempotencyKey;
	bool bAnonymous = false;
	bool bRefreshed = false;
	int32 Attempt = 0;
};
