#pragma once

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"

#include <cstdlib>

// Object/string/number/bool parser for the JSON the player session reads
// (sign-in replies, claims, and token replies). Arrays are skipped.

struct FJsonSerializer {
	struct Parser {
		const char* p;
		explicit Parser(const char* in) : p(in ? in : "") {}
		void Skip() { while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p; }
		bool Consume(char c) { Skip(); if (*p != c) return false; ++p; return true; }

		std::string ParseString() {
			if (!Consume('"')) return {};
			std::string out;
			while (*p && *p != '"') {
				if (*p == '\\' && p[1]) {
					++p;
					if (*p == 'n') out.push_back('\n');
					else if (*p == 't') out.push_back('\t');
					else out.push_back(*p);
					++p;
				} else {
					out.push_back(*p++);
				}
			}
			if (*p == '"') ++p;
			return out;
		}

		void SkipArray() {
			if (!Consume('[')) return;
			int depth = 1;
			while (*p && depth) {
				if (*p == '"') { ParseString(); continue; }
				if (*p == '[') ++depth;
				else if (*p == ']') --depth;
				if (depth) ++p;
			}
			if (*p == ']') ++p;
		}

		TSharedPtr<FJsonValue> ParseValue() {
			Skip();
			auto value = std::make_shared<FJsonValue>();
			if (*p == '"') {
				value->Type = EJson::String;
				value->String = FString(ParseString());
				return value;
			}
			if (*p == '{') {
				value->Type = EJson::Object;
				value->Object = ParseObject();
				return value;
			}
			if (*p == '[') { SkipArray(); value->Type = EJson::Array; return value; }
			if (std::strncmp(p, "true", 4) == 0) { p += 4; value->Type = EJson::Boolean; value->Bool = true; return value; }
			if (std::strncmp(p, "false", 5) == 0) { p += 5; value->Type = EJson::Boolean; value->Bool = false; return value; }
			if (std::strncmp(p, "null", 4) == 0) { p += 4; value->Type = EJson::Null; return value; }
			char* end = nullptr;
			value->Number = std::strtod(p, &end);
			if (end == p) return nullptr;
			p = end;
			value->Type = EJson::Number;
			return value;
		}

		TSharedPtr<FJsonObject> ParseObject() {
			auto obj = std::make_shared<FJsonObject>();
			if (!Consume('{')) return nullptr;
			Skip();
			if (Consume('}')) return obj;
			while (*p) {
				const std::string key = ParseString();
				if (!Consume(':')) return nullptr;
				TSharedPtr<FJsonValue> value = ParseValue();
				if (!value.IsValid()) return nullptr;
				obj->Values.Add(FString(key), value);
				Skip();
				if (Consume('}')) return obj;
				if (!Consume(',')) return nullptr;
			}
			return nullptr;
		}
	};

	static bool Deserialize(const TSharedRef<TJsonReader<>>& reader, TSharedPtr<FJsonObject>& out) {
		Parser parser(reader->Text.s.c_str());
		TSharedPtr<FJsonObject> obj = parser.ParseObject();
		if (!obj.IsValid()) return false;
		parser.Skip();
		if (*parser.p != '\0') return false;
		out = obj;
		return true;
	}
};
