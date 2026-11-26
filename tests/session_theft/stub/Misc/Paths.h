#pragma once

#include "CoreMinimal.h"

// FPaths::Combine / GetPath are string joins. ProjectSavedDir is the directory
// the test sets, which is what UBlaziumPlayerSession::DefaultSessionPath appends to.

struct FPaths {
	static std::string SavedDir;

	static FString ProjectSavedDir() { return FString(SavedDir.c_str()); }

	static FString GetPath(const FString& filename) {
		const size_t slash = filename.s.find_last_of('/');
		if (slash == std::string::npos) return FString();
		return FString(filename.s.substr(0, slash));
	}

	static FString Combine(const FString& a, const FString& b, const FString& c) {
		auto join = [](FString left, const FString& right) {
			if (left.IsEmpty()) return right;
			if (right.IsEmpty()) return left;
			if (!left.EndsWith("/")) left += "/";
			FString rest = right;
			while (rest.StartsWith("/")) rest.s.erase(rest.s.begin());
			left += rest;
			return left;
		};
		return join(join(a, b), c);
	}
};

inline std::string FPaths::SavedDir;
