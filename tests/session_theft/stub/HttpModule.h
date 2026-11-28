#pragma once

#include "Interfaces/IHttpRequest.h"

struct FHttpModule {
	static FHttpModule& Get() {
		static FHttpModule module;
		return module;
	}
	TSharedRef<IHttpRequest> CreateRequest() { return std::make_shared<IHttpRequest>(); }
};
