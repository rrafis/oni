#pragma once

#include <Platform.h>

#include <filesystem>
namespace vfs
{
	namespace fs = std::filesystem;
	struct Path
	{
		std::string Uri;
		std::string Root;
		std::string Subpath;

		Path() = default;
		Path(const fs::path& inPath);
		Path(const char* inPath);
		Path(const std::string& inPath);
		Path(const std::string& inRoot, const std::string& inSubpath);
		auto Resolve() const -> fs::path;
		auto Create() -> Path&;
		auto Exists() const -> bool;

		auto Filename() const -> std::string;
		auto Extension() const -> std::string;
		auto HasExtension() const -> bool;
		auto ReplaceFilename(const std::string& NewName = "") const -> Path;
		auto ReplaceExtension(const std::string& NewExt = "") const -> Path;
		auto ParentPath() const -> Path;
		auto RootPath() const -> Path;
		auto Canonicalize() -> void;

		Path& operator/=(const char* rhs);
		Path& operator/=(const std::string& rhs);
		Path& operator/=(const fs::path& rhs);
		Path& operator/=(const Path& rhs);

		inline operator std::string() const
		{
			return Resolve();
		}

		inline operator fs::path() const
		{
			return Resolve();
		}
	};

	auto operator==(const Path& Lhs, const Path& Rhs) -> bool;
	auto operator!=(const Path& Lhs, const Path& Rhs) -> bool;
	auto operator/(const vfs::Path& Lhs, const char* Rhs) -> Path;
	auto operator/(const vfs::Path& Lhs, const std::string& Rhs) -> Path;
	auto operator/(const vfs::Path& Lhs, const fs::path& Rhs) -> Path;
	auto operator/(const vfs::Path& Lhs, const vfs::Path& Rhs) -> Path;

	auto Mount(const std::string& VirtualRoot, const std::string& RealPath) -> void;
	auto Resolve(const vfs::Path& Path) -> fs::path;
	auto ResolveAll(const vfs::Path& Path) -> std::vector<fs::path>;
	auto Create(const vfs::Path& Path) -> void;
	auto Exists(const vfs::Path& Path) -> bool;
} // namespace vfs

template <>
struct std::formatter<vfs::Path> : std::formatter<std::string>
{
	auto format(const vfs::Path& p, format_context& ctx) const
	{
		return std::formatter<std::string>::format(p.Uri, ctx);
	}
};
