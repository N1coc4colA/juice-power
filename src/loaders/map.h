#ifndef LOADERS_MAP_H
#define LOADERS_MAP_H

#include <memory>
#include <string>
#include <unordered_map>

#include "src/loaders/enums.h"

namespace Graphics
{
//class Resources;
class Resources;
class Engine;
}

namespace World
{
class Scene;
}

namespace Loaders
{

/// @brief Parsed map JSON representation.
struct JsonMap;

/**
 * @brief Loads scene and resources from map files on disk.
 */
class Map
{
public:
    /// @brief Constructs loader from map root path.
    explicit Map(const std::string &path);

    /**
	 * @brief Loads the map & associated resources from path provided to ctor.
	 * @return The error status (Status::Ok if no error happened).
	 */
    auto load2(const std::shared_ptr<Graphics::Engine> &engine, const std::shared_ptr<World::Scene> &scene) -> std::tuple<Status, std::string>;

protected:
    /// @brief Builds graphics and physics resources from parsed map content.
    static auto buildResources(const std::unordered_map<std::string, int> &imagesMap,
                               const std::string &assetsDir,
                               const std::shared_ptr<Graphics::Resources> &resources,
                               const std::shared_ptr<Graphics::Engine> &engine,
                               const JsonMap &map) -> std::tuple<Status, std::string>;

private:
    /// @brief Filesystem path to the map directory.
    std::string m_path;
};


}


#endif // LOADERS_MAP_H
