#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// File writes follow Unreal Engine 4.18, the newest public engine tree whose
// Linux file implementation could be read. The plugin calls the two-argument
// FFileHelper::SaveStringToFile, whose defaults are AutoDetect and WriteFlags 0
// (Engine/Source/Runtime/Core/Public/Misc/FileHelper.h lines 56 and 76 in
// windystrife/UnrealEngine_NVIDIAGameWorks 4.18-GameWorks). That writer calls
// IFileManager::CreateFileWriter, which creates the parent directory and then
// IPlatformFile::OpenWrite with no permission argument
// (Engine/Source/Runtime/Core/Private/HAL/FileManagerGeneric.cpp lines 92-118).
// On Linux, OpenWrite uses open() with mode S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH
// (0644) and then ftruncate when not appending
// (Engine/Source/Runtime/Core/Private/Linux/LinuxPlatformFile.cpp lines 747-803).
// Directories are created with mkdir(..., 0755)
// (LinuxPlatformFile.cpp lines 827-829 and 944-987, and
// GenericPlatformFile.cpp CreateDirectoryTree lines 740-768).
// FSHA1::GetFileSHAHash only returns true for a name already in the signed-package
// map (SecureHash.cpp lines 892-906). A new session file is not in that map.

inline std::vector<std::string> g_files_opened_for_write;

inline bool ShaHashExists(const char*) { return false; }

inline bool DirectoryExistsAt(const std::string& path) {
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// IPlatformFile::CreateDirectoryTree (GenericPlatformFile.cpp lines 740-768)
// and FLinuxPlatformFile::CreateDirectory (LinuxPlatformFile.cpp lines 827-829).
inline bool CreateDirectoryTree(const std::string& directory) {
	if (directory.empty()) return false;
	std::string cur;
	for (size_t i = 0; i < directory.size(); ++i) {
		cur.push_back(directory[i]);
		if (directory[i] == '/' || i + 1 == directory.size()) {
			if (cur.empty() || cur == "/") continue;
			std::string name = cur;
			while (name.size() > 1 && name.back() == '/') name.pop_back();
			if (DirectoryExistsAt(name)) continue;
			if (mkdir(name.c_str(), 0755) != 0 && !DirectoryExistsAt(name)) return false;
		}
	}
	return DirectoryExistsAt(directory);
}

// FLinuxPlatformFile::CreateDirectoriesFromPath (LinuxPlatformFile.cpp lines 944-987).
inline bool CreateDirectoriesFromPath(const std::string& path) {
	struct stat st;
	if (stat(path.c_str(), &st) == 0) return true;
	std::string sub;
	for (size_t i = 0; i < path.size(); ++i) {
		sub.push_back(path[i]);
		if (path[i] != '/') continue;
		std::string dir = sub;
		if (stat(dir.c_str(), &st) == 0) continue;
		if (mkdir(dir.c_str(), 0755) != 0) return false;
	}
	return true;
}

struct FArchive {
	int fd = -1;
	explicit FArchive(int in) : fd(in) {}
	virtual ~FArchive() { Close(); }
	void Serialize(const void* data, int64 n) {
		const char* p = static_cast<const char*>(data);
		int64 off = 0;
		while (fd >= 0 && off < n) {
			const ssize_t wrote = ::write(fd, p + off, static_cast<size_t>(n - off));
			if (wrote < 0) break;
			off += wrote;
		}
	}
	bool Close() {
		if (fd >= 0) {
			::close(fd);
			fd = -1;
		}
		return true;
	}
};

struct IFileManager {
	static IFileManager& Get() {
		static IFileManager inst;
		return inst;
	}

	// FFileManagerGeneric::CreateFileWriter (FileManagerGeneric.cpp lines 92-118)
	// then FLinuxPlatformFile::OpenWrite (LinuxPlatformFile.cpp lines 747-803).
	// WriteFlags 0 means not append and not allow-read, so the flags are
	// O_CREAT|O_CLOEXEC|O_WRONLY and the file is truncated after open.
	FArchive* CreateFileWriter(const TCHAR* filename, uint32 write_flags) {
		if (ShaHashExists(filename)) return nullptr;
		const std::string path = filename ? filename : "";
		if (!CreateDirectoryTree(FPaths::GetPath(FString(path.c_str())).s)) return nullptr;
		const bool append = (write_flags & 0x08) != 0;
		const bool allow_read = (write_flags & 0x10) != 0;
		int flags = O_CREAT | O_CLOEXEC;
		flags |= allow_read ? O_RDWR : O_WRONLY;
		if (!CreateDirectoriesFromPath(path)) return nullptr;
		const int fd = ::open(path.c_str(), flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (fd < 0) return nullptr;
		if (!append) {
			if (ftruncate(fd, 0) != 0) {
				::close(fd);
				return nullptr;
			}
		}
		g_files_opened_for_write.push_back(path);
		return new FArchive(fd);
	}

	// FFileManagerGeneric::Delete (FileManagerGeneric.cpp lines 245-281).
	// RequireExists is false for the session clear, so a missing file is success.
	bool Delete(const TCHAR* filename, bool require_exists, bool even_read_only, bool) {
		(void)even_read_only;
		if (ShaHashExists(filename)) return false;
		const std::string path = filename ? filename : "";
		struct stat st;
		const bool exists = stat(path.c_str(), &st) == 0;
		if (require_exists && !exists) return false;
		if (exists && ::unlink(path.c_str()) != 0) return false;
		return true;
	}
};
