#include "resources.h"

#include <span>

#include "engine.h"


namespace Graphics
{

void Resources::build(Engine &engine)
{
	std::vector<uint32_t> indices {};
	const auto size = vertices.size();
	indices.reserve(size*4);

	// Fill in the indices.
	for (size_t i = 0; i < size; i++) {
		const auto v = i*4;

		indices.push_back(v);
		indices.push_back(v+1);
		indices.push_back(v+3);
		indices.push_back(v);
		indices.push_back(v+3);
		indices.push_back(v+2);
	}

	// a Vertices is an array of Vertex, so it's ok to cast it directly.
	meshBuffers = engine.uploadMesh(indices, std::span<Vertex>(reinterpret_cast<Vertex *>(vertices.data()), size*4));

	engine.mainDeletionQueue.push_function([&engine, this]() {
		engine.destroyBuffer(meshBuffers.indexBuffer);
		engine.destroyBuffer(meshBuffers.vertexBuffer);
	});
}


}
