#include "loadedgltf.h"

#include "node.h"


void LoadedGLTF::draw(const glm::mat4 &topMatrix, DrawContext &ctx)
{
	// create renderables from the scenenodes
	for (auto &n : topNodes) {
		n->draw(topMatrix, ctx);
	}
}

void LoadedGLTF::clearAll()
{
}
