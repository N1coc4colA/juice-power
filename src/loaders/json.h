#ifndef LOADERS_JSON_H
#define LOADERS_JSON_H

#include <cstdint>
#include <vector>
#include <string>
#include <array>


namespace Loaders
{

struct JsonResourceElement
{
	std::string name {};
	float w = 0.f;
	float h = 0.f;
	std::string source {};
	int type = -1;
};

struct JsonResourcesList
{
	std::vector<JsonResourceElement> resources {};
};

struct JsonChunkElement
{
	uint32_t type {};
	std::array<float, 3> position {0, 0, 0};
	bool ignores = false;
};

struct JsonMap
{
	std::string name = "<map>";
	bool chunksExternal = false;
	bool resourcesExternal = false;
	size_t chunksCount = 0;
	std::vector<JsonResourceElement> resources {};
	std::vector<std::vector<JsonChunkElement>> chunks {};
};


}


#endif // LOADERS_JSON_H
