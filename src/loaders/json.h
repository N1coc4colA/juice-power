#ifndef LOADERS_JSON_H
#define LOADERS_JSON_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Loaders
{

/*
 * The types must be std::array, std::vector, for containers
 * in order to be supported by Glaze's reflection system.
 */

/**
 * @brief Represents the JSON data associated to a Resource used by a map.
 * @note The members also define the JSON format to be used.
 */
struct JsonResourceElement
{
	/// @brief Element type, currently unused.
	int type = -1;

	/// @brief Name of the resource, should be a unique name.
	std::string name {};
	/// @brief Source image for the resource.
	std::string source {};

	/* Resource's basic physical data */
	/// @brief Physical width of the resource.
	float w = 0.f;
	/// @brief Physical height of the resource.
	float h = 0.f;

	/// @brief Mass of the elements of this resource type.
	float mass = 1.f;

	/// @brief Elesticity, bounciness of the resource.
	float elasticity = 1.f;

    /// @brief Animation's spritesheet grid size, ROW*COLUMNS
    std::array<float, 2> gridSize = {1.f, 1.f};

    /// @brief Animation frames count.
    uint16_t frames = 0;

    /// @brief Animation frame duration, interval between 2 frames.
    float interval = 0.1f;
};

struct JsonResourcesList
{
	std::vector<JsonResourceElement> resources {};
};

/**
 * @brief Represents the JSON data associated to an element of a chunk.
 * @note The members also define the JSON format to be used.
 */
struct JsonChunkElement
{
	uint32_t type {};
	/**
	 * @brief Location of the element in the chunk.
	 * @note The third value, Z, is the layer within the chunk, not affecting collisions.
	 */
	std::array<float, 3> position {0, 0, 0};

	/* Element's initial data. */
	/// @brief Initial angle of the element. Not used for now.
	float angle = 0.f;
	/// @brief Initial moment of inertia.
	float MoI = 1.f;

	/// @brief Initial velocity of the element.
	std::array<float, 2> velocity {};
	/// @brief Initial acceleration of the element.
	std::array<float, 2> acceleration {};
	/// @brief Initial angular velocity of the element.
	float angularVelocity = 0.f;

	/// @brief If the object is affected by collisions.
	bool canCollide = true;
	/// @brief If the object is subject to gravity.
	bool isNotFixed = true;

	// [TODO] See how to handle initial frictions, thrusts & torques...
};

/**
 * @brief Represents the JSON data associated to a map.
 * @note The members also define the JSON format to be used.
 */
struct JsonMap
{
	/// @brief Name of the map, should be unique.
	std::string name = "<map>";
	/// @brief Tells if the chunks are located in external files.
	bool chunksExternal = false;
	/// @brief Tells if the resources' data are located in a separate file.
	bool resourcesExternal = false;
	/**
	 * @brief Number of chunks in the map.
	 * @note Must be specified in JSON only if @variable chunksExternal is set to true.
	 * @note The value is never directly used to access in-memory chunk info.
	 */
	size_t chunksCount = 0;
	/**
	 * @brief Resources used in the map.
	 * @note Maybe unspecified in JSON (@variable resourcesExternal is false).
	 * Stores resources' info before loading all related data.
	 */
	std::vector<JsonResourceElement> resources {};
	/**
	 * @brief Chunks used in the map.
	 * @note Maybe unspecified in JSON (@variable chunksExternal is false).
	 * Stores chunks' info before loading all related data.
	 */
	std::vector<std::vector<JsonChunkElement>> chunks {};
    /**
     * @brief movable
     */
    std::vector<JsonChunkElement> movings{};
};


}


#endif // LOADERS_JSON_H
