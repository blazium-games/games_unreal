#pragma once

#include "CoreMinimal.h"
#include "HAL/FileManager.h"

// FFileHelper::SaveStringToFile / LoadFileToString from
// Engine/Source/Runtime/Core/Private/Misc/FileHelper.cpp lines 114-150 and 204-244
// (windystrife/UnrealEngine_NVIDIAGameWorks, tag 4.18-GameWorks).
// AutoDetect writes a pure-ASCII string as raw bytes (the else branch at lines 238-241).
// A session value is pure ASCII, so the file bytes are that value.
// Load uses BufferToString (lines 57-104): no BOM means the bytes are the string.

struct FFileHelper {
	enum class EEncodingOptions { AutoDetect, ForceAnsi, ForceUnicode, ForceUTF8, ForceUTF8WithoutBOM };
	enum class EHashOptions { None = 0 };

	static bool SaveStringToFile(const FString& string, const TCHAR* filename,
		EEncodingOptions encoding = EEncodingOptions::AutoDetect,
		IFileManager* files = &IFileManager::Get(), uint32 write_flags = 0) {
		TUniquePtr<FArchive> ar(files->CreateFileWriter(filename, write_flags));
		if (!ar) return false;
		if (string.IsEmpty()) return true;
		const bool unicode = encoding == EEncodingOptions::ForceUnicode
			|| (encoding == EEncodingOptions::AutoDetect && !FCString::IsPureAnsi(*string));
		if (encoding == EEncodingOptions::ForceUTF8 || encoding == EEncodingOptions::ForceUTF8WithoutBOM || !unicode) {
			if (encoding == EEncodingOptions::ForceUTF8) {
				const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
				ar->Serialize(bom, 3);
			}
			ar->Serialize(string.s.data(), static_cast<int64>(string.s.size()));
			return true;
		}
		// The session value is pure ASCII, so the plugin's default call does not take this branch.
		return false;
	}

	static bool LoadFileToString(FString& result, const TCHAR* filename, EHashOptions = EHashOptions::None) {
		const int fd = ::open(filename, O_RDONLY | O_CLOEXEC);
		if (fd < 0) return false;
		std::string bytes;
		char buf[4096];
		while (true) {
			const ssize_t n = ::read(fd, buf, sizeof buf);
			if (n < 0) { ::close(fd); return false; }
			if (n == 0) break;
			bytes.append(buf, buf + n);
		}
		::close(fd);
		if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF
			&& static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF) {
			bytes.erase(0, 3);
		}
		result = FString(bytes);
		return true;
	}
};
