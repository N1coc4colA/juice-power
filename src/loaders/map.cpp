#include "map.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <glaze/glaze.hpp>

#include <magic_enum.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "../graphics/engine.h"
#include "../graphics/resources.h"

#include "../world/scene.h"
#include "../world/chunk.h"

#include "json.h"


namespace fs = std::filesystem;


namespace Loaders
{

using Pixel = union __attribute__((packed)) { struct __attribute__((packed)) {unsigned char r, g, b, a;}; unsigned char data[4]; uint32_t value; };

Map::Map(const std::string &path)
	: m_path(path)
{
}

Status Map::load(Graphics::Engine &engine, World::Scene &m_scene)
{
	if (!fs::exists(m_path)) {
		return Status::MissingDirectory;
	}

	if (!fs::is_directory(m_path)) {
		return Status::NotDir;
	}

	const std::string m_assets = m_path + "/assets/";
	if (!fs::exists(m_assets)) {
		return Status::MissingDirectory;
	}

	if (!fs::is_directory(m_assets)) {
		return Status::NotDir;
	}

	std::vector<fs::path> paths {};
	std::vector<std::string> names {};
	const auto crop = m_path.length() +1;

	for (const auto &entry : fs::directory_iterator(m_path)) {
		const auto &p = entry.path();
		paths.push_back(p);
		names.push_back(p.string().substr(crop));
	}

	const auto mapAccess = std::find(names.cbegin(), names.cend(), "map.json");
	if (mapAccess == names.cend()) {
		return Status::MissingMapFile;
	}

	const auto pathsMapIndex = std::distance(names.cbegin(), mapAccess);
	std::ifstream f {};
	f.open(paths[pathsMapIndex], std::ifstream::in);
	if (!f.is_open()) {
		return Status::OpenError;
	}

	const auto size = fs::file_size(paths[pathsMapIndex]);
	std::string mapContent(size, '\0');
	f.read(&mapContent[0], size);

	const auto mapJson = glz::read_json<JsonMap>(mapContent);
	if (!mapJson.has_value()) {
		std::cout << "Failed to open file " << paths[pathsMapIndex] << ':' << mapJson.error().location << ':' << magic_enum::enum_name(mapJson.error().ec) << mapJson.error().includer_error << std::endl;
		return Status::JsonError;
	}

	JsonMap map = mapJson.value();

	// Now we may need to load from external JSON source file (Resources)
	// Or load from external sources the chunk files.

	// Load separate resources if relevant.
	if (map.resourcesExternal) {
		const auto resAccess = std::find(names.cbegin(), names.cend(), "resources.json");
		if (resAccess == names.cend()) {
			return Status::MissingJson;
		}

		const auto pathsResIndex = std::distance(names.cbegin(), resAccess);
		std::ifstream f {};
		f.open(paths[pathsResIndex], std::ifstream::in);
		if (!f.is_open()) {
			return Status::OpenError;
		}

		const auto size = std::filesystem::file_size(paths[pathsResIndex]);
		std::string resContent(size, '\0');
		f.read(&resContent[0], size);

		const auto resJson = glz::read_json<std::vector<JsonResourceElement>>(resContent);
		if (!resJson.has_value()) {
			std::cout << __LINE__;
			std::cout << "Failed to open file " << paths[pathsResIndex]
					  << ':' << resJson.error().location
					  << ':' << magic_enum::enum_name(resJson.error().ec)
					  << ':' << resJson.error().includer_error
					  << ':' << resJson.error().custom_error_message
					  << std::endl;
			return Status::JsonError;
		}

		const std::vector<JsonResourceElement> &res = resJson.value();
		map.resources = res;//.resources;
	}

	// Check that every resource does exist.
	for (const auto &res : map.resources) {
		if (!fs::exists(m_assets + res.source)) {
			return Status::MissingResource;
		}
	}

	if (map.chunksExternal) {
		const auto size = map.chunksCount;

		std::vector<std::vector<JsonChunkElement>> chunks {};

		// Check that all files exist
		for (auto i = 0; i < size; i++) {
			if (!fs::exists(m_path + '/' + std::to_string(i) + ".json")) {
				return Status::MissingJson;
			}
		}

		chunks.reserve(size);

		// Try to open & load all chunk files.
		for (auto i = 0; i < size; i++) {
			const std::string chunkName = m_path + '/' + std::to_string(i) + ".json";
			std::ifstream f {};
			f.open(chunkName, std::ifstream::in);
			if (!f.is_open()) {
				return Status::OpenError;
			}

			const auto size = fs::file_size(chunkName);
			std::string chunkContent(size, '\0');
			f.read(&chunkContent[0], size);

			const auto chunkJson = glz::read_json<std::vector<JsonChunkElement>>(chunkContent);
			if (!chunkJson.has_value()) {
				std::cout << __LINE__;
				std::cout << "Failed to open file " << chunkName
						  << ':' << chunkJson.error().location
						  << ':' << magic_enum::enum_name(chunkJson.error().ec)
						  << ':' << chunkJson.error().includer_error << std::endl;
				return Status::JsonError;
			}

			chunks.push_back(chunkJson.value());
		}

		// Now that we properly loaded it all, we just have to replace it.
		map.chunks = chunks;
	}

	// Now that any external resource have been check or loaded, we can generate the object.
	// First load the resources.

	m_scene.res = new Graphics::Resources {};

	std::map<std::string, int> mapped; // Maps name to resource ID.
	const auto resSize = map.resources.size();
	m_scene.res->images.reserve(resSize);
	m_scene.res->surfaces.reserve(resSize);
	m_scene.res->types.reserve(resSize);
	//m_scene.res->vertices.reserve(resSize);

	const std::vector<uint32_t> indices = {0, 1, 3, 0, 3, 2};

	size_t i = 0;
	for (const auto &res : map.resources) {
		// Trya load it.

		int width = 0,
			height = 0,
			channels = 0;
		stbi_uc *imgData = stbi_load((m_assets + res.source).c_str(), &width, &height, &channels, 4); // Force 4 channels (RGBA)

		if (!imgData) {
			return Status::OpenError;
		}

		const Graphics::Resources::Vertices vertices { .data = {
			{
				.position = {0.f, 0.f, 0.f},
				.uv_x = 0.f,
				.normal = {0.f, 0.f, 1.f},
				.uv_y = 0.f,
				.color = glm::vec4(0.f, 1.f, 0.f, 1.f),
			},
			{
				.position = {0.f, 1.f, 0.f},
				.uv_x = 0.f,
				.normal = {0.f, 0.f, 1.f},
				.uv_y = 1.f,
				.color = glm::vec4(1.f, 0.f, 0.f, 1.f),
			},
			{
				.position = {1.f, 0.f, 0.f},
				.uv_x = 1.f,
				.normal = {0.f, 0.f, 1.f},
				.uv_y = 0.f,
				.color = glm::vec4(0.f, 0.f, 1.f, 1.f),
			},
			{
				.position = {1.f, 1.f, 0.f},
				.uv_x = 1.f,
				.normal = {0.f, 0.f, 1.f},
				.uv_y = 1.f,
				.color = glm::vec4(1.f, 1.f, 0.f, 1.f),
			},
		}};

		// Now make the Vk Image.
		m_scene.res->types.push_back(res.type);
		m_scene.res->vertices.push_back(vertices);
		m_scene.res->images.push_back(engine.createImage(
			imgData,
			VkExtent3D {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_SAMPLED_BIT)
		);
		stbi_image_free(imgData);

		// [TODO] Here we should generate the surfaces for the physics engine.
		//m_scene.res->surfaces.push_back(...);

		// We succeed, let's add it.
		mapped.insert({res.name, i});
		i++;
	}

	m_scene.chunks.resize(map.chunks.size());
	const auto csize = map.chunks.size();
	for (auto i = 0; i < csize; i++) {
		auto &s = m_scene.chunks[i];
		const auto &c = map.chunks[i];
		const auto count = c.size();

		s.descriptions.reserve(count);
		s.positions.reserve(count);
		//s.transforms.reserve(count);

		for (const auto &e : c) {
			s.descriptions.push_back(e.type);
		}
		for (const auto &e : c) {
			s.positions.push_back(glm::vec3{e.position[0], e.position[1], e.position[2]});
		}
	}

	m_scene.res->build(engine);

	return Status::Ok;
}


}
