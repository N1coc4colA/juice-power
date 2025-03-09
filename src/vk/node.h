#ifndef NODE_H
#define NODE_H

#include <vector>
#include <memory>
#include <glm/glm.hpp>

#include "renderable.h"
#include "loader.h"


class Node
	: public Renderable
{
public:
	// parent pointer must be a weak pointer to avoid circular dependencies
	std::weak_ptr<Node> parent;
	std::vector<std::shared_ptr<Node>> children;

	glm::mat4 localTransform;
	glm::mat4 worldTransform;

	void refreshTransform(const glm::mat4 &parentMatrix);
	void draw(const glm::mat4 &topMatrix, DrawContext &ctx) override;
};

class MeshNode
	: public Node
{
public:
	std::shared_ptr<MeshAsset> mesh;

	void draw(const glm::mat4& topMatrix, DrawContext &ctx) override;
};


#endif // NODE_H
