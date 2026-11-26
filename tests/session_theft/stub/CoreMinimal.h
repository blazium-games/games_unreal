#pragma once

// Minimal stand-ins for the Unreal types UBlaziumPlayerSession.cpp names.
// File creation behavior is not here; Misc/FileHelper.h and HAL/FileManager.h
// follow the engine sources cited in those headers.

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using int32 = int;
using int64 = long long;
using uint8 = unsigned char;
using uint32 = unsigned int;
using TCHAR = char;

#define TEXT(x) x
#define BLAZIUMGAMES_API
#define CORE_API
#define UENUM(...)
#define USTRUCT(...)
#define UCLASS(...)
#define UFUNCTION(...)
#define UPROPERTY(...)
#define GENERATED_BODY() public: typedef UGameInstanceSubsystem Super;
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE(Name) struct Name { void Broadcast() {} }
#define check(x) ((void)0)
#define UE_LOG(...) ((void)0)

namespace ESPMode {
struct ThreadSafe {};
}

template <class T, class Mode = ESPMode::ThreadSafe>
struct TSharedPtr {
	std::shared_ptr<T> p;
	TSharedPtr() = default;
	TSharedPtr(std::nullptr_t) {}
	TSharedPtr(std::shared_ptr<T> in) : p(std::move(in)) {}
	bool IsValid() const { return static_cast<bool>(p); }
	T* Get() const { return p.get(); }
	T* operator->() const { return p.get(); }
	T& operator*() const { return *p; }
	explicit operator bool() const { return static_cast<bool>(p); }
};

template <class T, class Mode = ESPMode::ThreadSafe>
struct TSharedRef {
	std::shared_ptr<T> p;
	TSharedRef() = default;
	TSharedRef(std::shared_ptr<T> in) : p(std::move(in)) {}
	T* operator->() const { return p.get(); }
	T& operator*() const { return *p; }
};

template <class T>
struct TWeakObjectPtr {
	T* p = nullptr;
	TWeakObjectPtr() = default;
	TWeakObjectPtr(T* in) : p(in) {}
	TWeakObjectPtr& operator=(T* in) { p = in; return *this; }
	T* Get() const { return p; }
	bool IsValid() const { return p != nullptr; }
	void Reset() { p = nullptr; }
};

template <class T>
struct TArray {
	std::vector<T> v;
	int32 Num() const { return static_cast<int32>(v.size()); }
	void Add(const T& x) { v.push_back(x); }
	void Reset() { v.clear(); }
	void Empty() { v.clear(); }
	void clear() { v.clear(); }
	void push_back(const T& x) { v.push_back(x); }
	T& operator[](int i) { return v[i]; }
	const T& operator[](int i) const { return v[i]; }
};

struct FString {
	std::string s;
	FString() = default;
	FString(const char* in) : s(in ? in : "") {}
	FString(const std::string& in) : s(in) {}
	bool IsEmpty() const { return s.empty(); }
	void Reset() { s.clear(); }
	void Empty() { s.clear(); }
	int32 Len() const { return static_cast<int32>(s.size()); }
	const char* operator*() const { return s.c_str(); }
	bool operator==(const FString& o) const { return s == o.s; }
	bool operator!=(const FString& o) const { return s != o.s; }
	bool operator==(const char* o) const { return s == (o ? o : ""); }
	bool operator!=(const char* o) const { return !(*this == o); }
	bool operator<(const FString& o) const { return s < o.s; }
	FString& operator+=(const FString& o) { s += o.s; return *this; }
	FString& operator+=(const char* o) { s += o ? o : ""; return *this; }

	FString TrimStartAndEnd() const {
		size_t a = 0;
		size_t b = s.size();
		while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
		while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
		return FString(s.substr(a, b - a));
	}
	void ParseIntoArray(TArray<FString>& out, const char* delim, bool cull_empty) const {
		out.clear();
		const std::string d = delim ? delim : "";
		size_t start = 0;
		while (true) {
			const size_t pos = d.empty() ? std::string::npos : s.find(d, start);
			const std::string part = s.substr(start, pos == std::string::npos ? std::string::npos : pos - start);
			if (!cull_empty || !part.empty()) out.push_back(FString(part));
			if (pos == std::string::npos) break;
			start = pos + d.size();
		}
	}
	void ReplaceInline(const char* from, const char* to) {
		const std::string f = from ? from : "";
		const std::string t = to ? to : "";
		if (f.empty()) return;
		size_t pos = 0;
		while ((pos = s.find(f, pos)) != std::string::npos) {
			s.replace(pos, f.size(), t);
			pos += t.size();
		}
	}
	void AppendChar(char c) { s.push_back(c); }
	bool StartsWith(const char* prefix) const {
		const std::string p = prefix ? prefix : "";
		return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
	}
	bool EndsWith(const char* suffix) const {
		const std::string p = suffix ? suffix : "";
		return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
	}
	bool EndsWith(const FString& suffix) const { return EndsWith(suffix.s.c_str()); }
	void LeftChopInline(int n) {
		if (n >= Len()) s.clear();
		else s.resize(s.size() - static_cast<size_t>(n));
	}
	FString ToLower() const {
		FString out = *this;
		for (char& c : out.s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return out;
	}
	static FString Printf(const char* fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		va_list aq;
		va_copy(aq, ap);
		const int n = std::vsnprintf(nullptr, 0, fmt, ap);
		va_end(ap);
		std::string buf(n > 0 ? static_cast<size_t>(n) : 0, '\0');
		if (n > 0) std::vsnprintf(buf.data(), buf.size() + 1, fmt, aq);
		va_end(aq);
		return FString(buf);
	}
};

inline FString operator+(const FString& a, const FString& b) { FString r = a; r += b; return r; }
inline FString operator+(const FString& a, const char* b) { FString r = a; r += b; return r; }
inline FString operator+(const char* a, const FString& b) { FString r(a); r += b; return r; }

template <class K, class V>
struct TMap {
	std::map<K, V> m;
	V* Find(const K& k) {
		auto it = m.find(k);
		return it == m.end() ? nullptr : &it->second;
	}
	const V* Find(const K& k) const {
		auto it = m.find(k);
		return it == m.end() ? nullptr : &it->second;
	}
	void Add(const K& k, const V& v) { m[k] = v; }
	void Reset() { m.clear(); }
};

template <class Sig>
using TFunction = std::function<Sig>;

struct FMath {
	template <class T>
	static T Max(T a, T b) { return a > b ? a : b; }
};

struct FCString {
	static int32 Atoi(const TCHAR* s) { return s ? std::atoi(s) : 0; }
	static bool IsPureAnsi(const TCHAR* s) {
		if (!s) return true;
		for (; *s; ++s) {
			if (static_cast<unsigned char>(*s) > 127) return false;
		}
		return true;
	}
};

template <class T>
struct TUniquePtr {
	T* p = nullptr;
	explicit TUniquePtr(T* in = nullptr) : p(in) {}
	~TUniquePtr() { delete p; }
	TUniquePtr(const TUniquePtr&) = delete;
	T* operator->() const { return p; }
	explicit operator bool() const { return p != nullptr; }
};
