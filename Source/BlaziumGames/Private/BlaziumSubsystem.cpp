#include "BlaziumSubsystem.h"

#include "BlaziumApi.h"

namespace
{
UBlaziumSubsystem* GCallbackOwner = nullptr;
}

void UBlaziumSubsystem::Deinitialize()
{
	if (GCallbackOwner == this)
	{
		GCallbackOwner = nullptr;
	}
	Super::Deinitialize();
}

void UBlaziumSubsystem::Tick(float DeltaTime)
{
	(void)DeltaTime;
	if (GCallbackOwner != nullptr && !IsValid(GCallbackOwner))
	{
		GCallbackOwner = nullptr;
	}
	if (GCallbackOwner == nullptr)
	{
		GCallbackOwner = this;
	}
	if (GCallbackOwner != this)
	{
		return;
	}
	FBlaziumApi& Api = FBlaziumApi::Get();
	if (Api.IsClientSession())
	{
		Api.RunClientCallbacks();
	}
	else if (Api.IsServerSession())
	{
		Api.RunServerCallbacks();
	}
}

TStatId UBlaziumSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBlaziumSubsystem, STATGROUP_Tickables);
}

bool UBlaziumSubsystem::IsTickable() const
{
	const FBlaziumApi& Api = FBlaziumApi::Get();
	return Api.IsClientSession() || Api.IsServerSession();
}
