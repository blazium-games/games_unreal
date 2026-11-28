#pragma once

#include "CoreMinimal.h"
#include <ctime>

struct FDateTime {
	static FDateTime UtcNow() { return {}; }
	int64 ToUnixTimestamp() const { return static_cast<int64>(std::time(nullptr)); }
};
