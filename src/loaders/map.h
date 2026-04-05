#ifndef LOADERS_MAP_H
#define LOADERS_MAP_H

#include <memory>
#include <string>
#include <unordered_map>

#include "enums.h"

namespace Graphics
{
//class Resources;
class Resources2;
class Engine;
}

namespace World
{
class Scene;
}

namespace Loaders
{

struct JsonMap;

class Map
{
public:
    explicit Map(std::string path);

    /**
	 * @brief Loads the map & associated resources from path provided to ctor.
	 * @return The error status (Status::Ok if no error happened).
	 */
    auto load2(const std::shared_ptr<Graphics::Engine> &engine, const std::shared_ptr<World::Scene> &m_scene) -> std::tuple<Status, std::string>;

protected:
    static auto buildResources(const std::unordered_map<std::string, int> &imagesMap,
                               const std::string &m_assets,
                               const std::shared_ptr<Graphics::Resources2> &res2,
                               const std::shared_ptr<Graphics::Engine> &engine,
                               const JsonMap &map) -> std::tuple<Status, std::string>;

private:
    std::string m_path;
};


}


#endif // LOADERS_MAP_H
