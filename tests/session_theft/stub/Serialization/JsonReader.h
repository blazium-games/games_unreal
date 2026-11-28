#pragma once

#include "CoreMinimal.h"

template <class T = char>
struct TJsonReader {
	FString Text;
};

template <class T = char>
struct TJsonReaderFactory {
	static TSharedRef<TJsonReader<T>> Create(const FString& json) {
		auto reader = std::make_shared<TJsonReader<T>>();
		reader->Text = json;
		return reader;
	}
};
