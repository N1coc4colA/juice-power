#ifndef LOADERS_MAP_H
#define LOADERS_MAP_H

#include <string>


namespace Graphics
{
class Resources;
class Engine;
}

namespace World
{
class Scene;
}

namespace Loaders
{

enum Status
{
	Ok,
	OpenError,
	NotDir,
	MissingDirectory,
	JsonError,
	MissingJson,
	MissingMapFile,
	MissingChunkFile,
	MissingResourceFile,
	MissingResource,

	First = Ok,
	Last = MissingResource,
};


class Map
{
public:
	explicit Map(const std::string &path);

	/**
	 * @brief Loads the map & associated resources from path provided to ctor.
	 * @return The error status (Status::Ok if no error happened).
	 */
	Status load(Graphics::Engine &engine, World::Scene &m_scene);

private:
	const std::string m_path {};
};


}


#endif // LOADERS_MAP_H
