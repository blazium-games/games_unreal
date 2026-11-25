#include "BlaziumSapi.h"

#include "BlaziumApi.h"
#include "BlaziumPlayerSession.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Containers/Ticker.h"
#include "Misc/Guid.h"
#include "GenericPlatform/GenericPlatformHttp.h"

bool FBlaziumSapiRules::ValidLogic(const FString& Logic)
{
	if (Logic.IsEmpty() || Logic.Len() > 64)
	{
		return false;
	}
	for (const TCHAR C : Logic)
	{
		const bool bOk = (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('0') && C <= TEXT('9')) || C == TEXT('_') || C == TEXT('-');
		if (!bOk)
		{
			return false;
		}
	}
	return true;
}

FString FBlaziumSapiRules::NormalizeMethod(const FString& Method)
{
	const FString Verb = Method.IsEmpty() ? FString(TEXT("POST")) : Method.ToUpper();
	if (Verb == TEXT("GET") || Verb == TEXT("POST") || Verb == TEXT("PUT") || Verb == TEXT("PATCH") || Verb == TEXT("DELETE"))
	{
		return Verb;
	}
	return FString();
}

FString FBlaziumSapiRules::BuildUrl(const FString& BaseUrl, const FString& GameUid, const FString& Logic, const FString& Method, const FString& Body)
{
	FString Base = BaseUrl;
	while (Base.EndsWith(TEXT("/")))
	{
		Base.LeftChopInline(1);
	}
	FString Url = Base + TEXT("/v1/") + FGenericPlatformHttp::UrlEncode(GameUid) + TEXT("/") + Logic;
	if (Method == TEXT("GET") && !Body.IsEmpty())
	{
		FString Query = Body;
		while (Query.StartsWith(TEXT("?")))
		{
			Query.RightChopInline(1);
		}
		Url += TEXT("?") + Query;
	}
	return Url;
}

float FBlaziumSapiRules::RetryDelay(int32 Attempt, const FString& RetryAfter)
{
	float Wait = FCString::Atof(*RetryAfter);
	if (Wait <= 0.f)
	{
		Wait = 0.5f * FMath::Pow(2.f, static_cast<float>(Attempt));
	}
	return FMath::Min(Wait, 8.f);
}

FString FBlaziumSapiRules::ErrorKey(const FString& Body)
{
	const FString Needle = TEXT("\"key\"");
	int32 At = Body.Find(Needle, ESearchCase::CaseSensitive);
	if (At == INDEX_NONE)
	{
		return FString();
	}
	At += Needle.Len();
	const int32 Open = Body.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1);
	if (Open == INDEX_NONE)
	{
		return FString();
	}
	const int32 Close = Body.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 1);
	if (Close == INDEX_NONE)
	{
		return FString();
	}
	return Body.Mid(Open + 1, Close - Open - 1);
}

UBlaziumSapi* UBlaziumSapi::CallLogic(UObject* WorldContextObject, const FString& InLogic, const FString& InMethod, const FString& BodyJson, bool bInAnonymous)
{
	UBlaziumSapi* Action = NewObject<UBlaziumSapi>(WorldContextObject);
	Action->Logic = InLogic;
	Action->Method = InMethod;
	Action->Body = BodyJson;
	Action->bAnonymous = bInAnonymous;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UBlaziumSapi::Activate()
{
	if (!FBlaziumSapiRules::ValidLogic(Logic))
	{
		Finish(false, 0, FString(), TEXT("bad_request"));
		return;
	}
	Method = FBlaziumSapiRules::NormalizeMethod(Method);
	if (Method.IsEmpty())
	{
		Finish(false, 0, FString(), TEXT("bad_request"));
		return;
	}
	if (Method != TEXT("GET"))
	{
		if (Body.IsEmpty())
		{
			Body = TEXT("{}");
		}
		if (FTCHARToUTF8(*Body).Length() > FBlaziumSapiRules::MaxBodyBytes)
		{
			Finish(false, 0, FString(), TEXT("bad_request"));
			return;
		}
	}
	IdempotencyKey = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	Send();
}

void UBlaziumSapi::Send()
{
	// A player signed in with no launcher (UBlaziumPlayerSession) asks the session for its token.
	UBlaziumPlayerSession* Player = UBlaziumPlayerSession::Active();
	if (Player != nullptr && !FBlaziumApi::Get().IsClientSession() && !Player->GetGameUid().IsEmpty())
	{
		if (bAnonymous)
		{
			SendWithToken(Player->GetGameUid(), FString());
			return;
		}
		TWeakObjectPtr<UBlaziumSapi> Weak(this);
		const FString PlayerGame = Player->GetGameUid();
		Player->GetToken(TEXT("sapi"), [Weak, PlayerGame](bool bOk, const FString& PlayerToken, const FString& Code)
		{
			UBlaziumSapi* Self = Weak.Get();
			if (Self == nullptr)
			{
				return;
			}
			if (!bOk)
			{
				Self->Finish(false, 0, FString(), Code.IsEmpty() ? FString(TEXT("not_logged_on")) : Code);
				return;
			}
			Self->SendWithToken(PlayerGame, PlayerToken);
		});
		return;
	}
	FBlaziumApi& Api = FBlaziumApi::Get();
	const FString GameUid = Api.GetGameUid();
	if (GameUid.IsEmpty())
	{
		Finish(false, 0, FString(), TEXT("no_host"));
		return;
	}
	FString Token;
	if (!bAnonymous)
	{
		// The host hands out a scoped token for this game and audience. It is never the login token.
		const FString Reply = Api.RequestToken(TEXT("sapi"));
		const bool bOk = Reply.Contains(TEXT("\"ok\":true"));
		if (bOk)
		{
			const FString Needle = TEXT("\"token\":\"");
			const int32 At = Reply.Find(Needle, ESearchCase::CaseSensitive);
			if (At != INDEX_NONE)
			{
				const int32 Start = At + Needle.Len();
				const int32 End = Reply.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
				if (End != INDEX_NONE)
				{
					Token = Reply.Mid(Start, End - Start);
				}
			}
		}
		if (Token.IsEmpty())
		{
			FString Code = TEXT("not_logged_on");
			const FString CodeNeedle = TEXT("\"code\":\"");
			const int32 CodeAt = Reply.Find(CodeNeedle, ESearchCase::CaseSensitive);
			if (CodeAt != INDEX_NONE)
			{
				const int32 Start = CodeAt + CodeNeedle.Len();
				const int32 End = Reply.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
				if (End != INDEX_NONE)
				{
					Code = Reply.Mid(Start, End - Start);
				}
			}
			Finish(false, 0, FString(), Code);
			return;
		}
	}
	SendWithToken(GameUid, Token);
}

void UBlaziumSapi::SendWithToken(const FString& GameUid, const FString& Token)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(FBlaziumSapiRules::BuildUrl(BaseUrl, GameUid, Logic, Method, Body));
	Request->SetVerb(Method);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Blazium-Channel"), Channel.IsEmpty() ? FString(TEXT("stable")) : Channel.ToLower());
	if (Method != TEXT("GET"))
	{
		// Retries reuse the key, so a repeated call applies once.
		Request->SetHeader(TEXT("Idempotency-Key"), IdempotencyKey);
		Request->SetContentAsString(Body);
	}
	if (!Token.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
	}
	Request->OnProcessRequestComplete().BindUObject(this, &UBlaziumSapi::OnResponse);
	if (!Request->ProcessRequest())
	{
		Finish(false, 0, FString(), TEXT("service_unavailable"));
	}
}

void UBlaziumSapi::OnResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected)
{
	const int32 Status = (bConnected && Response.IsValid()) ? Response->GetResponseCode() : 0;
	const FString ResponseBody = (bConnected && Response.IsValid()) ? Response->GetContentAsString() : FString();
	if (Status == 401 && !bAnonymous && !bRefreshed)
	{
		// The token may have been revoked or aged out early. Ask once more.
		bRefreshed = true;
		FBlaziumApi::Get().ForgetTokens();
		if (UBlaziumPlayerSession* Player = UBlaziumPlayerSession::Active())
		{
			Player->ForgetTokens();
		}
		Send();
		return;
	}
	const bool bRetryable = Status == 0 || Status == 429 || Status == 503;
	if (bRetryable && Attempt < FBlaziumSapiRules::MaxRetries)
	{
		const FString RetryAfter = (bConnected && Response.IsValid()) ? Response->GetHeader(TEXT("Retry-After")) : FString();
		const float Delay = FBlaziumSapiRules::RetryDelay(Attempt, RetryAfter);
		++Attempt;
		TWeakObjectPtr<UBlaziumSapi> Weak(this);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float) {
			if (UBlaziumSapi* Self = Weak.Get())
			{
				Self->Send();
			}
			return false;
		}), Delay);
		return;
	}
	if (Status >= 200 && Status < 300)
	{
		Finish(true, Status, ResponseBody, FString());
		return;
	}
	FString Key = FBlaziumSapiRules::ErrorKey(ResponseBody);
	if (Key.IsEmpty())
	{
		Key = Status == 0 ? FString(TEXT("service_unavailable")) : FString::Printf(TEXT("http_%d"), Status);
	}
	Finish(false, Status, ResponseBody, Key);
}

void UBlaziumSapi::Finish(bool bSuccess, int32 Status, const FString& ResponseBody, const FString& ErrorKey)
{
	if (bSuccess)
	{
		OnSuccess.Broadcast(true, Status, ResponseBody, FString());
	}
	else
	{
		OnFailure.Broadcast(false, Status, ResponseBody, ErrorKey);
	}
	SetReadyToDestroy();
}
