#ifndef LOADERS_MAP_H
#define LOADERS_MAP_H

#include <cstdint>
#include <string>

#include "enums.h"

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
	std::string m_path;
};


}


#endif // LOADERS_MAP_H
