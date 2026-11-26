#pragma once

#include "CoreMinimal.h"

#include <vector>

struct IHttpRequest;

using FHttpRequestPtr = TSharedPtr<IHttpRequest>;

struct FHttpResponse {
	int code = 0;
	FString body;
	int32 GetResponseCode() const { return code; }
	FString GetContentAsString() const { return body; }
};

using FHttpResponsePtr = TSharedPtr<FHttpResponse>;

struct FHttpRequestCompleteDelegate {
	std::function<void(FHttpRequestPtr, FHttpResponsePtr, bool)> fn;
	template <class L>
	void BindLambda(L fn_in) { fn = std::move(fn_in); }
};

struct HttpCall {
	std::string verb;
	std::string url;
	std::string body;
	std::string authorization;
};

struct HttpReply {
	int status = 0;
	std::string body;
};

inline std::vector<HttpCall> g_http_calls;
inline std::function<HttpReply(const HttpCall&)> g_http_handler;

class IHttpRequest {
public:
	FString url;
	FString verb;
	FString body;
	std::map<std::string, FString> headers;
	FHttpRequestCompleteDelegate done;

	void SetURL(const FString& in) { url = in; }
	void SetVerb(const FString& in) { verb = in; }
	void SetHeader(const FString& key, const FString& value) { headers[key.s] = value; }
	void SetContentAsString(const FString& in) { body = in; }
	void SetTimeout(float) {}
	FHttpRequestCompleteDelegate& OnProcessRequestComplete() { return done; }

	bool ProcessRequest() {
		HttpCall call;
		call.verb = verb.s;
		call.url = url.s;
		call.body = body.s;
		auto auth = headers.find("Authorization");
		if (auth != headers.end()) call.authorization = auth->second.s;
		g_http_calls.push_back(call);
		HttpReply reply;
		if (g_http_handler) reply = g_http_handler(call);
		auto response = std::make_shared<FHttpResponse>();
		response->code = reply.status;
		response->body = FString(reply.body);
		const bool connected = reply.status != 0;
		if (done.fn) done.fn(FHttpRequestPtr(), FHttpResponsePtr(response), connected);
		return true;
	}
};
