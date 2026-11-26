#pragma once

#include "CoreMinimal.h"

enum class EJson : uint8 { None, Null, String, Number, Boolean, Array, Object };

struct FJsonObject;

struct FJsonValue {
	EJson Type = EJson::None;
	FString String;
	double Number = 0;
	bool Bool = false;
	TSharedPtr<FJsonObject> Object;
	FString AsString() const { return String; }
	double AsNumber() const { return Number; }
	bool AsBool() const { return Bool; }
};

struct FJsonObject {
	TMap<FString, TSharedPtr<FJsonValue>> Values;

	bool TryGetNumberField(const FString& key, double& out) const {
		const TSharedPtr<FJsonValue>* found = Values.Find(key);
		if (!found || !found->IsValid() || (*found)->Type != EJson::Number) return false;
		out = (*found)->Number;
		return true;
	}
	bool TryGetStringField(const FString& key, FString& out) const {
		const TSharedPtr<FJsonValue>* found = Values.Find(key);
		if (!found || !found->IsValid() || (*found)->Type != EJson::String) return false;
		out = (*found)->String;
		return true;
	}
	bool TryGetObjectField(const FString& key, const TSharedPtr<FJsonObject>*& out) {
		TSharedPtr<FJsonValue>* found = Values.Find(key);
		if (!found || !found->IsValid() || (*found)->Type != EJson::Object || !(*found)->Object.IsValid()) return false;
		out = &(*found)->Object;
		return true;
	}
};
