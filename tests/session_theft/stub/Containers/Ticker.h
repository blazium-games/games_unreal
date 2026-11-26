#pragma once

#include "CoreMinimal.h"

struct FTickerDelegate {
	std::function<bool(float)> fn;
	template <class L>
	static FTickerDelegate CreateLambda(L fn_in) {
		FTickerDelegate d;
		d.fn = std::move(fn_in);
		return d;
	}
};

struct FTSTicker {
	struct FDelegateHandle { int id = 0; };
	static FTSTicker& GetCoreTicker() {
		static FTSTicker ticker;
		return ticker;
	}
	// The real ticker would call back later. These tests call RefreshIfDue themselves.
	FDelegateHandle AddTicker(FTickerDelegate, float) { return {}; }
	void RemoveTicker(FDelegateHandle) {}
};
