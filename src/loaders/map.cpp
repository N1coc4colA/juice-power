#include "map.h"

#include <filesystem>

#include <glaze/glaze.hpp>


namespace fs = std::filesystem;


namespace Loaders
{

Map::Map(const std::string &path)
	: m_path(path)
{
}

Status Map::load()
{
	if (!std::filesystem::exists(m_path)) {
		return Status::MissingDirectory;
	}

	std::vector<fs::path> paths;
	std::vector<std::string> pathsStrings;
	const auto crop = m_path.length() +1;

	for (const auto &entry : fs::directory_iterator(m_path)) {
		const auto &p = entry.path();
		paths.push_back(p);
		pathsStrings.push_back(p.string().substr(crop));
	}

	if (std::find(pathsStrings.cbegin(), pathsStrings.cend(), "map.json") == pathsStrings.cend()) {
		return Status::MissingMapFile;
	}

	;
}


}
