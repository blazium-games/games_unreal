#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Interfaces/IHttpRequest.h"
#include "BlaziumPlayerSession.generated.h"

UENUM(BlueprintType)
enum class EBlaziumSignInState : uint8
{
	Idle,
	Starting,
	Waiting,
	Pending,
	Complete,
	Failed,
	Cancelled,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBlaziumSignInChanged);

// Plain helpers for the player session. No engine state, so they are easy to test.
struct BLAZIUMGAMES_API FBlaziumSessionRules
{
	// Reads who a session token is for and when it ends. The service decides whether it is valid.
	static bool ReadClaims(const FString& Jwt, FString& OutUser, int64& OutExp, int64& OutIat);
	// True in the last third of the session's life (an hour before the end when the issue time is unknown).
	// An ended session cannot renew, and a failed try waits until NextTry.
	static bool RefreshDue(int64 Now, int64 Issued, int64 Expires, int64 NextTry);
	// Seconds to keep a scoped token short of its end, so it is never sent as it expires.
	static int32 TokenMargin(int32 ExpiresIn);
	// Reads a string, number or bool field of a reply, looking at the top level and then inside "data".
	static FString JsonField(const FString& Json, const FString& Key);
	// The words of a refusal: "error" when it is text, else error.message, else message.
	static FString Refusal(const FString& Body);
	// "sapi", "lobby" or "server". Empty when the audience is not one of them.
	static FString NormalizeAudience(const FString& Audience);
};

// What a dedicated server or backend learns about a player.
USTRUCT(BlueprintType)
struct BLAZIUMGAMES_API FBlaziumPlayerProof
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blazium")
	bool bOk = false;

	UPROPERTY(BlueprintReadOnly, Category = "Blazium")
	FString UserUid;

	UPROPERTY(BlueprintReadOnly, Category = "Blazium")
	FString GameUid;

	UPROPERTY(BlueprintReadOnly, Category = "Blazium")
	FString Code;

	UPROPERTY(BlueprintReadOnly, Category = "Blazium")
	FString Error;
};

// The way into the Blazium IRC server. Token is the password: a secret that works for ten minutes.
struct BLAZIUMGAMES_API FBlaziumChatAccess
{
	bool bOk = false;
	FString Username;
	FString Token;
	FString Host;
	int32 Port = 0;
	bool bTls = true;
	FString WebSocket;
	FString Code;
	FString Error;
};

// A dedicated server (or any backend) asks who a player is, from the "server" token the player's game
// sent it. It needs the game's server key. Each token works once, so a leaked one cannot be replayed.
//
//   On the player's game:  Session->GetToken(TEXT("server"), ...)  -> send the token to the server
//   On the server:         FBlaziumPlayerVerifier::Verify(ServerKey, Token, [](const FBlaziumPlayerProof& P) { ... });
struct BLAZIUMGAMES_API FBlaziumPlayerVerifier
{
	static void Verify(const FString& ServerKey, const FString& PlayerToken, TFunction<void(const FBlaziumPlayerProof&)> Done, const FString& ApiBase = FString());
};

// Signs a player in with no Blazium launcher and no native library: a game shipped to web, mobile, console
// or any store. The player opens the link (or enters the short code at blazium.games/connect on another
// device) and signs in or creates an account there. The game never sees a password.
//
// This is a game instance subsystem. Configure it once with the game's uid, then:
//
//   UBlaziumPlayerSession* Session = GetGameInstance()->GetSubsystem<UBlaziumPlayerSession>();
//   Session->Configure(TEXT("<game uid>"));
//   Session->RestoreSaved();              // a returning player is signed in again with no steps
//   if (!Session->IsSignedIn()) { Session->StartSignIn(); }   // then show GetLoginUrl() and GetUserCode()
//
// UBlaziumSapi uses this session by itself once the player is signed in. A token is never a Blueprint value.
UCLASS()
class BLAZIUMGAMES_API UBlaziumPlayerSession : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// The session of this process, or null when none is running.
	static UBlaziumPlayerSession* Active();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// The game's uid is the project UUID from the store page.
	UFUNCTION(BlueprintCallable, Category = "Blazium|Player")
	void Configure(const FString& InGameUid);

	// Uses the sign-in an earlier run saved. An ended one is dropped.
	UFUNCTION(BlueprintCallable, Category = "Blazium|Player")
	void RestoreSaved();

	// Starts a sign-in and returns at once. Watch OnChanged and GetState. Asking while one runs does nothing.
	UFUNCTION(BlueprintCallable, Category = "Blazium|Player")
	void StartSignIn();

	UFUNCTION(BlueprintCallable, Category = "Blazium|Player")
	void CancelSignIn();

	// Ends the session on the service and forgets the saved copy.
	UFUNCTION(BlueprintCallable, Category = "Blazium|Player")
	void SignOut();

	UFUNCTION(BlueprintPure, Category = "Blazium|Player")
	bool IsSignedIn() const { return !Session.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Blazium|Player")
	EBlaziumSignInState GetState() const { return State; }

	// The link to open in a browser, or show as a QR code.
	UFUNCTION(BlueprintPure, Category = "Blazium|Player")
	FString GetLoginUrl() const { return LoginUrl; }

	// The short code for a device with no browser, entered at GetVerificationUrl() on another device.
	UFUNCTION(BlueprintPure, Category = "Blazium|Player")
	FString GetUserCode() const { return UserCode; }

	UFUNCTION(BlueprintPure, Category = "Blazium|Player")
	FString GetVerificationUrl() const { return VerificationUrl; }

	// The step the player still has to finish on the website (for example "legal"), while Pending.
	UFUNCTION(BlueprintPure, Category = "Blazium|Player")
	FString GetGate() const { return Gate; }

	UFUNCTION(BlueprintPure, Category = "Blazium|Player")
	FString GetError() const { return Error; }

	UFUNCTION(BlueprintPure, Category = "Blazium|Player")
	FString GetUserUid() const { return UserUid; }

	UFUNCTION(BlueprintPure, Category = "Blazium|Player")
	FString GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Blazium|Player")
	FString GetUsername() const { return Username; }

	// Raised whenever the state, the sign-in details or the signed-in player change.
	UPROPERTY(BlueprintAssignable, Category = "Blazium|Player")
	FBlaziumSignInChanged OnChanged;

	// The short-lived token a service wants ("sapi", "lobby" or "server"). Renews the session first when it
	// is due and reuses a token until a minute before it ends. Never hands out the session itself.
	// C++ only. A token is a secret: do not log it or save it.
	void GetToken(const FString& Audience, TFunction<void(bool bOk, const FString& Token, const FString& Code)> Done);

	// Signs the player in to the Blazium IRC server: asks for a ten minute password (never the session).
	// It needs a Blazium account with a username; a Steam player with no account has no chat. C++ only.
	void GetChatAccess(TFunction<void(const FBlaziumChatAccess&)> Done);

	// Drops cached tokens, so the next GetToken asks again (after a 401).
	void ForgetTokens();

	// Renews the session when it is due, then calls Next. Safe to call often.
	void RefreshIfDue(TFunction<void()> Next);

	// The session, for a platform secure store. Treat it as a password. Empty when signed out.
	FString ExportSession() const { return Session; }

	// Uses a session the game kept elsewhere. Returns empty when it was accepted, else why not.
	FString UseSession(const FString& Saved);

	FString GetGameUid() const { return GameUid; }

	// Where the services are. Set before StartSignIn for a local service.
	FString ApiBase = TEXT("https://api.blazium.online/api/v1");
	FString LoginBase = TEXT("https://login.blazium.online");

	// Seconds the login service may hold one poll.
	int32 PollSeconds = 10;

	// Where the session is kept between runs. The defaults write a file under Saved/Blazium, which is NOT
	// encrypted: on a phone or console set these to the Keychain, the Keystore or protected save data.
	TFunction<FString()> LoadSavedSession;
	TFunction<void(const FString&)> SaveSession;
	TFunction<void()> ClearSavedSession;

	// Seconds since the epoch. A test replaces it.
	TFunction<int64()> Clock;

private:
	struct FCachedToken
	{
		FString Token;
		int64 Until = 0;
		int32 Epoch = 0;
	};

	void Http(const FString& Verb, const FString& Url, const FString& Body, const FString& Bearer, float TimeoutSeconds, TFunction<void(int32 Status, const FString& Body)> Done);
	void After(float Seconds, TFunction<void()> Fn);
	void Poll(int32 Mine, const FString& Cid, const FString& Secret, const FString& Seen, int64 Started, int32 Failures);
	void GiveBack(const FString& Cid, const FString& Secret);
	bool Adopt(const FString& Jwt, const FString& InDisplayName, bool bSave);
	void Drop(EBlaziumSignInState Next);
	void SetState(EBlaziumSignInState Next);
	void Fail(int32 Mine, const FString& Why);
	void LoadProfile();
	void Notify();
	FString DefaultSessionPath() const;
	int64 Now() const;

	EBlaziumSignInState State = EBlaziumSignInState::Idle;
	FString GameUid;
	FString Session;
	FString LoginUrl;
	FString UserCode;
	FString VerificationUrl;
	FString Gate;
	FString Error;
	FString UserUid;
	FString DisplayName;
	FString Username;
	int64 Expires = 0;
	int64 Issued = 0;
	int64 NextRefreshTry = 0;
	int32 Generation = 0;
	int32 Epoch = 0;
	bool bRefreshing = false;
	TMap<FString, FCachedToken> Cache;
	FTSTicker::FDelegateHandle KeepFreshHandle;

	static TWeakObjectPtr<UBlaziumPlayerSession> ActiveSession;
};
