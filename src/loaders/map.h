#ifndef LOADERS_MAP_H
#define LOADERS_MAP_H

#include <string>
#include <vector>


namespace World
{
class Resources;
class Chunk;
}


namespace Loaders
{

enum Status
{
	Ok,
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

	Status load();

private:
	const std::string m_path;
};


}


#endif // LOADERS_MAP_H
