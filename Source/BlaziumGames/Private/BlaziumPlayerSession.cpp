#include "BlaziumPlayerSession.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "GenericPlatform/GenericPlatformHttp.h"

TWeakObjectPtr<UBlaziumPlayerSession> UBlaziumPlayerSession::ActiveSession;

namespace
{
TSharedPtr<FJsonObject> ParseObject(const FString& Json)
{
	if (Json.IsEmpty())
	{
		return nullptr;
	}
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		return nullptr;
	}
	return Object;
}

bool ValueText(const TSharedPtr<FJsonObject>& Object, const FString& Key, FString& Out)
{
	const TSharedPtr<FJsonValue>* Found = Object->Values.Find(Key);
	if (Found == nullptr || !Found->IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonValue>& Value = *Found;
	switch (Value->Type)
	{
	case EJson::String:
		Out = Value->AsString();
		return true;
	case EJson::Number:
		Out = FString::Printf(TEXT("%lld"), static_cast<int64>(Value->AsNumber()));
		return true;
	case EJson::Boolean:
		Out = Value->AsBool() ? TEXT("true") : TEXT("false");
		return true;
	default:
		return false;
	}
}

FString JsonEscape(const FString& Value)
{
	FString Out = Value;
	Out.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Out.ReplaceInline(TEXT("\""), TEXT("\\\""));
	return Out;
}

FString TrimSlash(const FString& Base)
{
	FString Out = Base;
	while (Out.EndsWith(TEXT("/")))
	{
		Out.LeftChopInline(1);
	}
	return Out;
}
}

// ---- rules ---------------------------------------------------------------------------------------

FString FBlaziumSessionRules::JsonField(const FString& Json, const FString& Key)
{
	const TSharedPtr<FJsonObject> Object = ParseObject(Json);
	if (!Object.IsValid())
	{
		return FString();
	}
	FString Out;
	if (ValueText(Object, Key, Out))
	{
		return Out;
	}
	const TSharedPtr<FJsonObject>* Data = nullptr;
	if (Object->TryGetObjectField(TEXT("data"), Data) && Data != nullptr && Data->IsValid() && ValueText(*Data, Key, Out))
	{
		return Out;
	}
	return FString();
}

FString FBlaziumSessionRules::Refusal(const FString& Body)
{
	const TSharedPtr<FJsonObject> Object = ParseObject(Body);
	if (Object.IsValid())
	{
		FString Message;
		if (ValueText(Object, TEXT("error"), Message) && !Message.IsEmpty())
		{
			return Message;
		}
		const TSharedPtr<FJsonObject>* Nested = nullptr;
		if (Object->TryGetObjectField(TEXT("error"), Nested) && Nested != nullptr && Nested->IsValid()
			&& ValueText(*Nested, TEXT("message"), Message) && !Message.IsEmpty())
		{
			return Message;
		}
		if (ValueText(Object, TEXT("message"), Message) && !Message.IsEmpty())
		{
			return Message;
		}
	}
	return TEXT("the request was refused");
}

FString FBlaziumSessionRules::NormalizeAudience(const FString& Audience)
{
	const FString Want = Audience.TrimStartAndEnd().ToLower();
	if (Want == TEXT("sapi") || Want == TEXT("lobby") || Want == TEXT("server"))
	{
		return Want;
	}
	return FString();
}

bool FBlaziumSessionRules::ReadClaims(const FString& Jwt, FString& OutUser, int64& OutExp, int64& OutIat)
{
	OutUser.Reset();
	OutExp = 0;
	OutIat = 0;
	if (Jwt.IsEmpty())
	{
		return false;
	}
	TArray<FString> Parts;
	Jwt.ParseIntoArray(Parts, TEXT("."), false);
	if (Parts.Num() != 3)
	{
		return false;
	}
	FString B64 = Parts[1];
	B64.ReplaceInline(TEXT("-"), TEXT("+"));
	B64.ReplaceInline(TEXT("_"), TEXT("/"));
	while (B64.Len() % 4 != 0)
	{
		B64.AppendChar(TEXT('='));
	}
	FString Payload;
	if (!FBase64::Decode(B64, Payload))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Claims = ParseObject(Payload);
	if (!Claims.IsValid())
	{
		return false;
	}
	double Number = 0;
	if (Claims->TryGetNumberField(TEXT("exp"), Number))
	{
		OutExp = static_cast<int64>(Number);
	}
	if (Claims->TryGetNumberField(TEXT("iat"), Number))
	{
		OutIat = static_cast<int64>(Number);
	}
	if (!Claims->TryGetStringField(TEXT("user_uid"), OutUser) || OutUser.IsEmpty())
	{
		Claims->TryGetStringField(TEXT("uid"), OutUser);
	}
	return OutExp > 0 && !OutUser.IsEmpty();
}

bool FBlaziumSessionRules::RefreshDue(int64 Now, int64 Issued, int64 Expires, int64 NextTry)
{
	if (Now >= Expires || Now < NextTry)
	{
		return false;
	}
	const int64 Margin = (Issued > 0 && Expires > Issued) ? (Expires - Issued) / 3 : 3600;
	return Now >= Expires - Margin;
}

int32 FBlaziumSessionRules::TokenMargin(int32 ExpiresIn)
{
	return ExpiresIn > 120 ? 60 : ExpiresIn / 2;
}

// ---- verifier --------------------------------------------------------------------------------------

void FBlaziumPlayerVerifier::Verify(const FString& ServerKey, const FString& PlayerToken, TFunction<void(const FBlaziumPlayerProof&)> Done, const FString& ApiBase)
{
	FBlaziumPlayerProof Refused;
	if (ServerKey.IsEmpty() || PlayerToken.IsEmpty())
	{
		Refused.Code = TEXT("bad_request");
		Refused.Error = TEXT("a server key and a player token are required");
		Done(Refused);
		return;
	}
	const FString Base = TrimSlash(ApiBase.IsEmpty() ? FString(TEXT("https://api.blazium.online/api/v1")) : ApiBase);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Base + TEXT("/server/verify-player"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Server-Key"), ServerKey);
	Request->SetContentAsString(TEXT("{\"token\":\"") + JsonEscape(PlayerToken) + TEXT("\"}"));
	Request->OnProcessRequestComplete().BindLambda([Done](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		FBlaziumPlayerProof Proof;
		const int32 Status = (bConnected && Response.IsValid()) ? Response->GetResponseCode() : 0;
		const FString Body = (bConnected && Response.IsValid()) ? Response->GetContentAsString() : FString();
		if (Status == 0)
		{
			Proof.Code = TEXT("service_unavailable");
			Proof.Error = TEXT("the Blazium service could not be reached");
		}
		else if (Status == 409)
		{
			Proof.Code = TEXT("already_used");
			Proof.Error = TEXT("this player token was already used");
		}
		else if (Status >= 200 && Status < 300 && !FBlaziumSessionRules::JsonField(Body, TEXT("user_uid")).IsEmpty())
		{
			Proof.bOk = true;
			Proof.UserUid = FBlaziumSessionRules::JsonField(Body, TEXT("user_uid"));
			Proof.GameUid = FBlaziumSessionRules::JsonField(Body, TEXT("game_uid"));
		}
		else
		{
			Proof.Code = Status == 401 || Status == 403 ? FString(TEXT("refused")) : FString::Printf(TEXT("http_%d"), Status);
			Proof.Error = FBlaziumSessionRules::Refusal(Body);
		}
		Done(Proof);
	});
	if (!Request->ProcessRequest())
	{
		Refused.Code = TEXT("service_unavailable");
		Refused.Error = TEXT("the request could not be started");
		Done(Refused);
	}
}

// ---- session ---------------------------------------------------------------------------------------

UBlaziumPlayerSession* UBlaziumPlayerSession::Active()
{
	return ActiveSession.Get();
}

void UBlaziumPlayerSession::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ActiveSession = this;
	Clock = []() { return FDateTime::UtcNow().ToUnixTimestamp(); };
	LoadSavedSession = [this]()
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *DefaultSessionPath());
		return Text.TrimStartAndEnd();
	};
	SaveSession = [this](const FString& Saved)
	{
		FFileHelper::SaveStringToFile(Saved, *DefaultSessionPath());
	};
	ClearSavedSession = [this]()
	{
		IFileManager::Get().Delete(*DefaultSessionPath(), false, true, true);
	};
	// Renews the session a little before it runs out, for as long as the game runs.
	TWeakObjectPtr<UBlaziumPlayerSession> Weak(this);
	KeepFreshHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
	{
		if (UBlaziumPlayerSession* Self = Weak.Get())
		{
			if (!Self->Session.IsEmpty())
			{
				Self->RefreshIfDue([]() {});
			}
		}
		return true;
	}), 30.f);
}

void UBlaziumPlayerSession::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(KeepFreshHandle);
	++Generation; // a sign-in in progress stops at its next step
	if (ActiveSession.Get() == this)
	{
		ActiveSession.Reset();
	}
	Super::Deinitialize();
}

FString UBlaziumPlayerSession::DefaultSessionPath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Blazium"), TEXT("session-") + GameUid + TEXT(".txt"));
}

int64 UBlaziumPlayerSession::Now() const
{
	return Clock ? Clock() : FDateTime::UtcNow().ToUnixTimestamp();
}

void UBlaziumPlayerSession::Configure(const FString& InGameUid)
{
	GameUid = InGameUid.TrimStartAndEnd();
}

void UBlaziumPlayerSession::Notify()
{
	OnChanged.Broadcast();
}

void UBlaziumPlayerSession::SetState(EBlaziumSignInState Next)
{
	State = Next;
	Notify();
}

void UBlaziumPlayerSession::Http(const FString& Verb, const FString& Url, const FString& Body, const FString& Bearer, float TimeoutSeconds, TFunction<void(int32, const FString&)> Done)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(Verb);
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	if (!Body.IsEmpty())
	{
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Request->SetContentAsString(Body);
	}
	if (!Bearer.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Bearer);
	}
	if (TimeoutSeconds > 0.f)
	{
		Request->SetTimeout(TimeoutSeconds);
	}
	Request->OnProcessRequestComplete().BindLambda([Done](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		const int32 Status = (bConnected && Response.IsValid()) ? Response->GetResponseCode() : 0;
		Done(Status, (bConnected && Response.IsValid()) ? Response->GetContentAsString() : FString());
	});
	if (!Request->ProcessRequest())
	{
		Done(0, FString());
	}
}

void UBlaziumPlayerSession::After(float Seconds, TFunction<void()> Fn)
{
	TWeakObjectPtr<UBlaziumPlayerSession> Weak(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, Fn](float)
	{
		if (Weak.IsValid())
		{
			Fn();
		}
		return false;
	}), Seconds);
}

// ---- saved sign-in -----------------------------------------------------------------------------

void UBlaziumPlayerSession::RestoreSaved()
{
	const FString Saved = LoadSavedSession ? LoadSavedSession() : FString();
	if (Saved.IsEmpty())
	{
		return;
	}
	if (!Adopt(Saved, FString(), false))
	{
		if (ClearSavedSession)
		{
			ClearSavedSession();
		}
		return;
	}
	SetState(EBlaziumSignInState::Complete);
	TWeakObjectPtr<UBlaziumPlayerSession> Weak(this);
	RefreshIfDue([Weak]()
	{
		if (UBlaziumPlayerSession* Self = Weak.Get())
		{
			Self->LoadProfile();
		}
	});
}

FString UBlaziumPlayerSession::UseSession(const FString& Saved)
{
	if (!Adopt(Saved, FString(), true))
	{
		return TEXT("that session is not usable (it has ended or is not a Blazium session)");
	}
	SetState(EBlaziumSignInState::Complete);
	return FString();
}

// ---- sign-in -------------------------------------------------------------------------------------

void UBlaziumPlayerSession::StartSignIn()
{
	if (!Session.IsEmpty())
	{
		SetState(EBlaziumSignInState::Complete);
		return;
	}
	if (State == EBlaziumSignInState::Starting || State == EBlaziumSignInState::Waiting || State == EBlaziumSignInState::Pending)
	{
		return;
	}
	LoginUrl.Reset();
	UserCode.Reset();
	VerificationUrl.Reset();
	Gate.Reset();
	Error.Reset();
	if (GameUid.IsEmpty())
	{
		Error = TEXT("the game uid is not set; call Configure first");
		SetState(EBlaziumSignInState::Failed);
		return;
	}
	const int32 Mine = ++Generation;
	SetState(EBlaziumSignInState::Starting);
	TWeakObjectPtr<UBlaziumPlayerSession> Weak(this);
	Http(TEXT("POST"), TrimSlash(LoginBase) + TEXT("/api/v1/session"), TEXT("{\"game_uid\":\"") + JsonEscape(GameUid) + TEXT("\"}"), FString(), 20.f,
		[Weak, Mine](int32 Status, const FString& Body)
		{
			UBlaziumPlayerSession* Self = Weak.Get();
			if (Self == nullptr || Mine != Self->Generation)
			{
				return;
			}
			if (Status == 0)
			{
				Self->Fail(Mine, TEXT("the sign-in service could not be reached"));
				return;
			}
			if (Status != 200)
			{
				Self->Fail(Mine, FBlaziumSessionRules::Refusal(Body));
				return;
			}
			const FString Cid = FBlaziumSessionRules::JsonField(Body, TEXT("cid"));
			const FString Secret = FBlaziumSessionRules::JsonField(Body, TEXT("secret"));
			if (Cid.IsEmpty() || Secret.IsEmpty())
			{
				Self->Fail(Mine, TEXT("the sign-in service gave an unexpected answer"));
				return;
			}
			Self->LoginUrl = FBlaziumSessionRules::JsonField(Body, TEXT("login_url"));
			Self->UserCode = FBlaziumSessionRules::JsonField(Body, TEXT("user_code"));
			Self->VerificationUrl = FBlaziumSessionRules::JsonField(Body, TEXT("verification_url"));
			Self->SetState(EBlaziumSignInState::Waiting);
			Self->Poll(Mine, Cid, Secret, TEXT("waiting"), Self->Now(), 0);
		});
}

void UBlaziumPlayerSession::Poll(int32 Mine, const FString& Cid, const FString& Secret, const FString& Seen, int64 Started, int32 Failures)
{
	if (Mine != Generation)
	{
		GiveBack(Cid, Secret);
		return;
	}
	if (Now() - Started > 31 * 60)
	{
		Fail(Mine, TEXT("the sign-in took too long"));
		return;
	}
	const int32 Wait = FMath::Max(1, PollSeconds);
	const FString Url = TrimSlash(LoginBase) + TEXT("/api/v1/session/") + FGenericPlatformHttp::UrlEncode(Cid)
		+ FString::Printf(TEXT("?wait=%d&seen=%s"), Wait, *Seen);
	TWeakObjectPtr<UBlaziumPlayerSession> Weak(this);
	Http(TEXT("GET"), Url, FString(), Secret, static_cast<float>(Wait + 10),
		[Weak, Mine, Cid, Secret, Seen, Started, Failures](int32 Status, const FString& Body)
		{
			UBlaziumPlayerSession* Self = Weak.Get();
			if (Self == nullptr)
			{
				return;
			}
			if (Mine != Self->Generation)
			{
				Self->GiveBack(Cid, Secret);
				return;
			}
			if (Status == 0)
			{
				if (Failures + 1 >= 8)
				{
					Self->Fail(Mine, TEXT("the sign-in service could not be reached"));
					return;
				}
				Self->After(static_cast<float>(Failures + 1), [Weak, Mine, Cid, Secret, Seen, Started, Failures]()
				{
					if (UBlaziumPlayerSession* Again = Weak.Get())
					{
						Again->Poll(Mine, Cid, Secret, Seen, Started, Failures + 1);
					}
				});
				return;
			}
			if (Status == 404)
			{
				Self->Fail(Mine, TEXT("the sign-in expired; start again"));
				return;
			}
			if (Status != 200)
			{
				Self->Fail(Mine, FBlaziumSessionRules::Refusal(Body));
				return;
			}
			const FString NowState = FBlaziumSessionRules::JsonField(Body, TEXT("state"));
			FString NextSeen = Seen;
			if (NowState == TEXT("complete"))
			{
				if (!Self->Adopt(FBlaziumSessionRules::JsonField(Body, TEXT("jwt")), FBlaziumSessionRules::JsonField(Body, TEXT("display_name")), true))
				{
					Self->Fail(Mine, TEXT("the sign-in service gave a session that could not be used"));
					return;
				}
				Self->Gate.Reset();
				Self->Error.Reset();
				Self->SetState(EBlaziumSignInState::Complete);
				Self->LoadProfile();
				return;
			}
			if (NowState == TEXT("pending"))
			{
				Self->Gate = FBlaziumSessionRules::JsonField(Body, TEXT("gate"));
				NextSeen = TEXT("pending");
				Self->SetState(EBlaziumSignInState::Pending);
			}
			Self->After(0.25f, [Weak, Mine, Cid, Secret, NextSeen, Started]()
			{
				if (UBlaziumPlayerSession* Again = Weak.Get())
				{
					Again->Poll(Mine, Cid, Secret, NextSeen, Started, 0);
				}
			});
		});
}

void UBlaziumPlayerSession::GiveBack(const FString& Cid, const FString& Secret)
{
	Http(TEXT("DELETE"), TrimSlash(LoginBase) + TEXT("/api/v1/session/") + FGenericPlatformHttp::UrlEncode(Cid), FString(), Secret, 10.f,
		[](int32, const FString&) {});
}

void UBlaziumPlayerSession::CancelSignIn()
{
	if (State == EBlaziumSignInState::Starting || State == EBlaziumSignInState::Waiting || State == EBlaziumSignInState::Pending)
	{
		++Generation; // the poll in flight gives the sign-in back when it returns
		LoginUrl.Reset();
		UserCode.Reset();
		VerificationUrl.Reset();
		Gate.Reset();
		SetState(EBlaziumSignInState::Cancelled);
	}
}

void UBlaziumPlayerSession::SignOut()
{
	const FString Old = Session;
	++Generation; // ends any sign-in in progress
	Drop(EBlaziumSignInState::Idle);
	if (Old.IsEmpty())
	{
		return;
	}
	Http(TEXT("POST"), TrimSlash(ApiBase) + TEXT("/private/logout"), FString(), Old, 10.f, [](int32, const FString&) {});
}

void UBlaziumPlayerSession::Fail(int32 Mine, const FString& Why)
{
	if (Mine != Generation)
	{
		return;
	}
	Error = Why;
	LoginUrl.Reset();
	UserCode.Reset();
	VerificationUrl.Reset();
	Gate.Reset();
	SetState(EBlaziumSignInState::Failed);
}

// ---- keeping the session fresh ---------------------------------------------------------------------

void UBlaziumPlayerSession::RefreshIfDue(TFunction<void()> Next)
{
	if (Session.IsEmpty() || bRefreshing || !FBlaziumSessionRules::RefreshDue(Now(), Issued, Expires, NextRefreshTry))
	{
		Next();
		return;
	}
	bRefreshing = true;
	const FString Used = Session;
	TWeakObjectPtr<UBlaziumPlayerSession> Weak(this);
	Http(TEXT("POST"), TrimSlash(ApiBase) + TEXT("/public/token/refresh"), FString(), Used, 20.f,
		[Weak, Used, Next](int32 Status, const FString& Body)
		{
			UBlaziumPlayerSession* Self = Weak.Get();
			if (Self == nullptr)
			{
				return;
			}
			Self->bRefreshing = false;
			if (Used != Self->Session)
			{
				Next(); // the player signed out or in again meanwhile
				return;
			}
			if (Status == 401 || Status == 403)
			{
				Self->Drop(EBlaziumSignInState::Idle);
				Next();
				return;
			}
			const FString Fresh = (Status >= 200 && Status < 300) ? FBlaziumSessionRules::JsonField(Body, TEXT("token")) : FString();
			if (Fresh.IsEmpty() || !Self->Adopt(Fresh, FString(), true))
			{
				Self->NextRefreshTry = Self->Now() + 60;
			}
			Next();
		});
}

// ---- tokens ---------------------------------------------------------------------------------------

void UBlaziumPlayerSession::GetToken(const FString& Audience, TFunction<void(bool, const FString&, const FString&)> Done)
{
	const FString Want = FBlaziumSessionRules::NormalizeAudience(Audience);
	if (Want.IsEmpty())
	{
		Done(false, FString(), TEXT("bad_audience"));
		return;
	}
	if (Session.IsEmpty())
	{
		Done(false, FString(), TEXT("not_logged_on"));
		return;
	}
	TWeakObjectPtr<UBlaziumPlayerSession> Weak(this);
	RefreshIfDue([Weak, Want, Done]()
	{
		UBlaziumPlayerSession* Self = Weak.Get();
		if (Self == nullptr)
		{
			return;
		}
		if (Self->Session.IsEmpty())
		{
			Done(false, FString(), TEXT("not_logged_on"));
			return;
		}
		if (const FCachedToken* Held = Self->Cache.Find(Want))
		{
			if (Held->Epoch == Self->Epoch && Self->Now() < Held->Until)
			{
				Done(true, Held->Token, FString());
				return;
			}
		}
		const FString Used = Self->Session;
		const int32 AskedAt = Self->Epoch;
		Self->Http(TEXT("POST"), TrimSlash(Self->ApiBase) + TEXT("/private/games/") + FGenericPlatformHttp::UrlEncode(Self->GameUid) + TEXT("/tokens"),
			TEXT("{\"audience\":\"") + Want + TEXT("\"}"), Used, 20.f,
			[Weak, Want, Done, Used, AskedAt](int32 Status, const FString& Body)
			{
				UBlaziumPlayerSession* Again = Weak.Get();
				if (Again == nullptr)
				{
					return;
				}
				if (Status == 401)
				{
					if (Used == Again->Session)
					{
						Again->Drop(EBlaziumSignInState::Idle);
					}
					Done(false, FString(), TEXT("not_logged_on"));
					return;
				}
				// The service answers {"data":{"token":...}}; JsonField looks inside the wrapper.
				const FString Token = (Status >= 200 && Status < 300) ? FBlaziumSessionRules::JsonField(Body, TEXT("token")) : FString();
				if (Token.IsEmpty())
				{
					FString Code = FBlaziumSessionRules::JsonField(Body, TEXT("code"));
					if (Code.IsEmpty())
					{
						Code = Status == 0 ? FString(TEXT("service_unavailable")) : FString::Printf(TEXT("http_%d"), Status);
					}
					Done(false, FString(), Code);
					return;
				}
				const int32 ExpiresIn = FCString::Atoi(*FBlaziumSessionRules::JsonField(Body, TEXT("expires_in")));
				if (Again->Epoch == AskedAt)
				{
					FCachedToken Entry;
					Entry.Token = Token;
					Entry.Epoch = AskedAt;
					Entry.Until = Again->Now() + FMath::Max(0, ExpiresIn - FBlaziumSessionRules::TokenMargin(ExpiresIn));
					Again->Cache.Add(Want, Entry);
				}
				Done(true, Token, FString());
			});
	});
}

void UBlaziumPlayerSession::ForgetTokens()
{
	Cache.Reset();
}

// ---- who the player is ----------------------------------------------------------------------------

void UBlaziumPlayerSession::LoadProfile()
{
	if (Session.IsEmpty())
	{
		return;
	}
	const FString Used = Session;
	TWeakObjectPtr<UBlaziumPlayerSession> Weak(this);
	Http(TEXT("GET"), TrimSlash(ApiBase) + TEXT("/private/account"), FString(), Used, 20.f, [Weak, Used](int32 Status, const FString& Body)
	{
		UBlaziumPlayerSession* Self = Weak.Get();
		if (Self == nullptr || Used != Self->Session)
		{
			return;
		}
		if (Status == 401)
		{
			Self->Drop(EBlaziumSignInState::Idle);
			return;
		}
		if (Status < 200 || Status >= 300)
		{
			return;
		}
		const FString Name = FBlaziumSessionRules::JsonField(Body, TEXT("display_name"));
		if (!Name.IsEmpty())
		{
			Self->DisplayName = Name;
		}
		Self->Username = FBlaziumSessionRules::JsonField(Body, TEXT("username"));
		Self->Notify();
	});
}

// ---- internals ------------------------------------------------------------------------------------

bool UBlaziumPlayerSession::Adopt(const FString& Jwt, const FString& InDisplayName, bool bSave)
{
	FString User;
	int64 Exp = 0;
	int64 Iat = 0;
	if (!FBlaziumSessionRules::ReadClaims(Jwt, User, Exp, Iat) || Exp <= Now())
	{
		return false;
	}
	if (User != UserUid)
	{
		DisplayName.Reset();
		Username.Reset();
	}
	Session = Jwt;
	UserUid = User;
	Expires = Exp;
	Issued = Iat;
	if (!InDisplayName.IsEmpty())
	{
		DisplayName = InDisplayName;
	}
	++Epoch;
	Cache.Reset();
	if (bSave && SaveSession)
	{
		SaveSession(Jwt);
	}
	Notify();
	return true;
}

void UBlaziumPlayerSession::Drop(EBlaziumSignInState Next)
{
	Session.Reset();
	Expires = 0;
	Issued = 0;
	UserUid.Reset();
	DisplayName.Reset();
	Username.Reset();
	++Epoch;
	Cache.Reset();
	if (ClearSavedSession)
	{
		ClearSavedSession();
	}
	SetState(Next);
}
