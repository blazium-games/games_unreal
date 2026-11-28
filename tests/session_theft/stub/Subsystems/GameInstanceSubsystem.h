#pragma once

#include "CoreMinimal.h"

struct FSubsystemCollectionBase {};

class UObject {
public:
	virtual ~UObject() = default;
};

class UGameInstanceSubsystem : public UObject {
public:
	virtual void Initialize(FSubsystemCollectionBase&) {}
	virtual void Deinitialize() {}
};
