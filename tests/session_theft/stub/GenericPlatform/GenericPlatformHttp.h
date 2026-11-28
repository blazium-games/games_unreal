#pragma once

#include "CoreMinimal.h"

// FGenericPlatformHttp::UrlEncode from
// Engine/Source/Runtime/Online/HTTP/Private/GenericPlatform/GenericPlatformHttp.cpp
// lines 59-106 (windystrife/UnrealEngine_NVIDIAGameWorks, tag 4.18-GameWorks).
// Unreserved characters stay as themselves. Every other byte is %HH uppercase.

struct FGenericPlatformHttp {
	static bool Allowed(unsigned char c) {
		static const char* allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
		for (const char* p = allowed; *p; ++p) if (static_cast<unsigned char>(*p) == c) return true;
		return false;
	}

	static FString UrlEncode(const FString& unencoded) {
		FString encoded;
		for (unsigned char byte : unencoded.s) {
			if (Allowed(byte)) {
				encoded.AppendChar(static_cast<char>(byte));
			} else if (byte != '\0') {
				encoded += TEXT("%");
				encoded += FString::Printf(TEXT("%.2X"), byte);
			}
		}
		return encoded;
	}
};
