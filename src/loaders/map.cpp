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

#include "../algorithms.h"

#include "json.h"


namespace fs = std::filesystem;

using namespace algorithms;


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
	m_scene.res->types.reserve(resSize);
	m_scene.res->borders.reserve(resSize);
	m_scene.res->normals.reserve(resSize);

	//const std::vector<uint32_t> indices = {0, 1, 3, 0, 3, 2};

	ImageVectorizer vectorizer {};

	size_t i = 0;
	for (const auto &res : map.resources) {
		// Trya load it.

		int width = 0,
			height = 0,
			channels = 0;

		// This gives an 8-bit per channel.
		stbi_uc *imgData = stbi_load((m_assets + res.source).c_str(), &width, &height, &channels, 4); // Force 4 channels (RGBA)

		if (!imgData) {
			return Status::OpenError;
		}

		const auto &h = res.h;
		const auto &w = res.w;
		const Graphics::Resources::Vertices vertices { .data = {
			{
				.position = {0.f, 0.f, 0.f},
				.uv_x = 0.f,
				.normal = {0.f, 0.f, 1.f},
				.uv_y = 0.f,
				.color = glm::vec4(0.f, 1.f, 0.f, 1.f),
			},
			{
				.position = {0.f, h, 0.f},
				.uv_x = 0.f,
				.normal = {0.f, 0.f, 1.f},
				.uv_y = 1.f,
				.color = glm::vec4(1.f, 0.f, 0.f, 1.f),
			},
			{
				.position = {w, 0.f, 0.f},
				.uv_x = 1.f,
				.normal = {0.f, 0.f, 1.f},
				.uv_y = 0.f,
				.color = glm::vec4(0.f, 0.f, 1.f, 1.f),
			},
			{
				.position = {w, h, 0.f},
				.uv_x = 1.f,
				.normal = {0.f, 0.f, 1.f},
				.uv_y = 1.f,
				.color = glm::vec4(1.f, 1.f, 0.f, 1.f),
			},
		}};

		// Because each pixel is the @var channels values, mult width by @var channels.
		vectorizer.determineImageBorders(MatrixView(imgData, width*channels, height), channels);

		for (auto &p : vectorizer.points) {
			p.x *= w;
		}

		for (auto &p : vectorizer.points) {
			p.y *= h;
		}

		m_scene.res->types.push_back(res.type);
		m_scene.res->vertices.push_back(std::move(vertices));
		m_scene.res->borders.push_back(std::move(vectorizer.points));
		m_scene.res->normals.push_back(std::move(vectorizer.normals));

		// Now make the Vk Image.
		m_scene.res->images.push_back(engine.createImage(
			imgData,
			VkExtent3D {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_SAMPLED_BIT)
		);

		stbi_image_free(imgData);

		// We succeed, let's add it.
		mapped.insert({res.name, i});
		i++;
	}

	m_scene.chunks.resize(map.chunks.size());
	const auto csize = map.chunks.size();

	// Setup transform matrices
	for (auto i = 0; i < csize; i++) {
		auto &s = m_scene.chunks[i];
		const auto &c = map.chunks[i];
		const auto count = c.size();

		// At first, all objects have their usual transform matrix.
		s.transforms = std::vector(count, glm::mat4{1.f});
	}

	// Reserve space for our vectors.
	for (auto i = 0; i < csize; i++) {
		auto &s = m_scene.chunks[i];
		const auto count = map.chunks[i].size();

		// We fill it in later to avoid constructing and then change the data.
		s.descriptions.reserve(count);
		s.positions.reserve(count);
	}

	for (auto i = 0; i < csize; i++) {
		const auto count = map.chunks[i].size();

		m_scene.chunks[i].entities.resize(count);
	}

	// Fill in basic data
	for (auto i = 0; i < csize; i++) {
		const auto &c = map.chunks[i];
		auto &s = m_scene.chunks[i];

		for (const auto &e : c) {
			s.descriptions.push_back(e.type);
		}
		for (const auto &e : c) {
			s.positions.push_back(glm::vec3{e.position[0], e.position[1], e.position[2]});
		}
	}

	// Update entities' information.
	for (auto i = 0; i < csize; i++) {
		const auto &c = map.chunks[i];
		auto &s = m_scene.chunks[i];

		const auto esize = s.entities.size();
		for (auto j = 0; j < esize; j++) {
			s.entities[j].mass = map.resources[s.descriptions[j]].mass;
		}

		for (auto j = 0; j < esize; j++) {
			const auto &e = c[j];

			s.entities[j].position = glm::vec2{e.position[0], e.position[1]};
		}

		for (auto j = 0; j < esize; j++) {
			const auto &e = c[j];

			s.entities[j].velocity = glm::vec2{e.velocity[0], e.velocity[1]};
		}

		for (auto j = 0; j < esize; j++) {
			const auto &e = c[j];

			s.entities[j].acceleration = glm::vec2{e.acceleration[0], e.acceleration[1]};
		}

		for (auto j = 0; j < esize; j++) {
			s.entities[j].angularVelocity = c[j].angularVelocity;
		}

		for (auto j = 0; j < esize; j++) {
			s.entities[j].MoI = c[j].MoI;
		}

		for (auto j = 0; j < esize; j++) {
			s.entities[j].mass = map.resources[s.descriptions[j]].mass;
		}

		for (auto j = 0; j < esize; j++) {
			s.entities[j].borders = m_scene.res->borders[s.descriptions[j]];
		}

		for (auto j = 0; j < esize; j++) {
			s.entities[j].normals = m_scene.res->normals[s.descriptions[j]];
		}
	}

	m_scene.res->build(engine);

	m_scene.view = m_scene.chunks | std::ranges::views::drop(0) | std::ranges::views::take(std::min(size_t(2), csize));

	return Status::Ok;
}


}
