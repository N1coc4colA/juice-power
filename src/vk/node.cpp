#include "node.h"

#include "renderobject.h"


void Node::refreshTransform(const glm::mat4 &parentMatrix)
{
	worldTransform = parentMatrix * localTransform;
	for (auto c : children) {
		c->refreshTransform(worldTransform);
	}
}

void Node::draw(const glm::mat4 &topMatrix, DrawContext &ctx)
{
	// draw children
	for (auto &c : children) {
		c->draw(topMatrix, ctx);
	}
}

void MeshNode::draw(const glm::mat4 &topMatrix, DrawContext &ctx)
{
	const glm::mat4 nodeMatrix = topMatrix * worldTransform;

	for (const auto &s : mesh->surfaces) {
		const RenderObject def {
			.indexCount = s.count,
			.firstIndex = s.startIndex,
			.indexBuffer = mesh->meshBuffers.indexBuffer.buffer,
			.material = &s.material->data,
			.transform = nodeMatrix,
			.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress,
		};

		ctx.OpaqueSurfaces.push_back(def);
	}

	// recurse down
	Node::draw(topMatrix, ctx);
}
