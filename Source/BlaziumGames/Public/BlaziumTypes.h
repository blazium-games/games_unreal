#pragma once

#include "CoreMinimal.h"
#include "BlaziumTypes.generated.h"

UENUM(BlueprintType)
enum class EBlaziumInitResult : uint8
{
	OK = 0,
	FailedGeneric = 1,
	NoHost = 2,
	VersionMismatch = 3,
	NoConfig = 4,
	InvalidConfig = 5,
	NotLoggedOn = 6,
	UnknownGame = 7,
	NotOwned = 8,
};
