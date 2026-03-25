#include "Vfs.h"

#include <fstream>

namespace vfs
{
	using MountType = std::unordered_map<std::string, std::vector<fs::path>>;
	struct FState
	{
		MountType Mounts;
	};
	static FState State;

	Path::Path(const fs::path& inPath) : Path(inPath.string()) {}

	Path::Path(const char* inPath) : Path(std::string(inPath)) {}

	Path::Path(const std::string& inPath) : Uri(inPath)
	{
		auto pos = Uri.find("://");
		if (pos != std::string::npos)
		{
			Root = Uri.substr(0, pos);
			Subpath = Uri.substr(pos + 3);
		}
		// TODO: Handle error here;
	}

	Path::Path(const std::string& inRoot, const std::string& inSubpath) : Root(inRoot), Subpath(inSubpath)
	{
		Uri = Root + "://" + Subpath;
	}

	auto Path::Resolve() const -> fs::path
	{
		return vfs::Resolve(*this);
	}

	auto Path::Create() -> Path&
	{
		vfs::Create(*this);
		return *this;
	}

	auto Path::Exists() const -> bool
	{
		return vfs::Exists(*this);
	}

	auto Path::Filename() const -> std::string
	{
		return fs::path(Subpath).filename().string();
	}

	auto Path::Extension() const -> std::string
	{
		return fs::path(Subpath).extension().string();
	}

	auto Path::HasExtension() const -> bool
	{
		return fs::path(Subpath).has_extension();
	}

	auto Path::ReplaceFilename(const std::string& NewName) const -> Path
	{
		fs::path tmp(Subpath);
		tmp.replace_filename(NewName);
		return Path(Root, tmp);
	}

	auto Path::ReplaceExtension(const std::string& NewExt) const -> Path
	{
		fs::path tmp(Subpath);
		tmp.replace_extension(NewExt);
		return Path(Root, tmp);
	}

	auto Path::ParentPath() const -> Path
	{
		fs::path tmp(Subpath);
		tmp = tmp.parent_path();
		return Path(Root, tmp);
	}

	auto Path::RootPath() const -> Path
	{
		return Path(Root, "");
	}

	auto Path::serialize(cereal::JSONInputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Uri);

			auto pos = Uri.find("://");
			if (pos != std::string::npos)
			{
				Root = Uri.substr(0, pos);
				Subpath = Uri.substr(pos + 3);
			}
		}
	}

	auto Path::serialize(cereal::JSONOutputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Uri);
		}
	}

	Path& Path::operator/=(const char* rhs)
	{
		fs::path tmp(Subpath);
		tmp /= rhs;
		Subpath = tmp.string();
		Uri = Root + "://" + Subpath;
		return *this;
	}

	Path& Path::operator/=(const std::string& rhs)
	{
		fs::path tmp(Subpath);
		tmp /= rhs;
		Subpath = tmp.string();
		Uri = Root + "://" + Subpath;
		return *this;
	}

	Path& Path::operator/=(const fs::path& rhs)
	{
		fs::path tmp(Subpath);
		tmp /= rhs;
		Subpath = tmp.string();
		Uri = Root + "://" + Subpath;
		return *this;
	}

	Path& Path::operator/=(const Path& rhs)
	{
		fs::path tmp(Subpath);
		tmp /= rhs.Subpath;
		Subpath = tmp.string();
		Uri = Root + "://" + Subpath;
		return *this;
	}

	auto operator==(const Path& Lhs, const Path& Rhs) -> bool
	{
		return Lhs.Root == Rhs.Root && Lhs.Subpath == Rhs.Subpath;
	}

	auto operator!=(const Path& Lhs, const Path& Rhs) -> bool
	{
		return !(Lhs == Rhs);
	}

	auto operator/(const vfs::Path& Lhs, const char* Rhs) -> Path
	{
		vfs::Path Res = Lhs;
		return Res /= Rhs;
	}

	auto operator/(const vfs::Path& Lhs, const std::string& Rhs) -> Path
	{
		vfs::Path Res = Lhs;
		return Res /= Rhs;
	}

	auto operator/(const vfs::Path& Lhs, const fs::path& Rhs) -> Path
	{
		vfs::Path Res = Lhs;
		return Res /= Rhs;
	}

	auto operator/(const vfs::Path& Lhs, const vfs::Path& Rhs) -> Path
	{
		vfs::Path Res = Lhs;
		return Res /= Rhs;
	}

	auto Mount(const std::string& VirtualRoot, const std::string& RealPath) -> void
	{
		State.Mounts[VirtualRoot].push_back(RealPath);
	}

	auto Resolve(const vfs::Path& Path) -> fs::path
	{
		// TODO(Rafis): Assert here that Path.Root and Path.Subpath not an empty strings.

		auto& Mounts = State.Mounts;

		auto it = Mounts.find(Path.Root);
		if (it == Mounts.end())
		{
			return {};
		}

		for (const auto& realRoot : it->second)
		{
			fs::path candidate = realRoot / fs::path(Path.Subpath);
			if (fs::exists(candidate))
				return candidate;
		}

		return {};
	}

	auto ResolveAll(const vfs::Path& Path) -> std::vector<fs::path>
	{
		// TODO(Rafis): Assert here that Path.Root and Path.Subpath not an empty strings.

		auto& Mounts = State.Mounts;
		std::vector<fs::path> results;

		auto it = Mounts.find(Path.Root);
		if (it == Mounts.end())
		{
			return results;
		}

		for (const auto& realRoot : it->second)
		{
			fs::path candidate = realRoot / fs::path(Path.Subpath);
			if (fs::exists(candidate))
			{
				results.push_back(candidate);
			}
		}

		return results;
	}

	auto Create(const vfs::Path& Path) -> void
	{
		// TODO(Rafis): Assert here that Path.Root and Path.Subpath not an empty strings.

		auto& Mounts = State.Mounts;

		auto it = Mounts.find(Path.Root);
		if (it == Mounts.end())
		{
			return;
		}

		for (const auto& realRoot : it->second)
		{
			fs::path candidate = realRoot / fs::path(Path.Subpath);
			if (fs::exists(candidate))
			{
				return;
			}
			else
			{
				// Ensure parent directories exist
				fs::path parent = candidate.parent_path();
				if (!parent.empty())
				{
					std::error_code ec;
					fs::create_directories(parent, ec);
					if (ec) // failed to create parent dirs
					{
						std::cout << "Failed to create directory: " << ec.message() << "\n";
						continue;
					}
				}

				// Decide file vs directory based on extension
				if (candidate.has_extension())
				{
					// Create an empty file
					std::ofstream(candidate.string()).close();
				}
				else
				{
					// Create a directory
					std::error_code ec;
					fs::create_directory(candidate, ec);
					if (ec) // failed to create directory
					{
						std::cout << "Failed to create directory: " << ec.message() << "\n";
						continue;
					}
				}

				return;
			}
		}
	}

	auto Exists(const vfs::Path& Path) -> bool
	{
		// TODO(Rafis): Assert here that Path.Root and Path.Subpath not an empty strings.

		auto& Mounts = State.Mounts;

		auto it = Mounts.find(Path.Root);
		if (it == Mounts.end())
		{
			return false;
		}

		for (const auto& realRoot : it->second)
		{
			fs::path candidate = realRoot / fs::path(Path.Subpath);
			if (fs::exists(candidate))
				return true;
		}

		return false;
	}
} // namespace vfs
