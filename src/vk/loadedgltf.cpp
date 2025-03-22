#include "loadedgltf.h"

#include "node.h"
#include "engine.h"


void LoadedGLTF::draw(const glm::mat4 &topMatrix, DrawContext &ctx)
{
	// create renderables from the scenenodes
	for (auto &n : topNodes) {
		n->draw(topMatrix, ctx);
	}
}

void LoadedGLTF::clearAll()
{
	VkDevice dv = creator->_device;

	descriptorPool.destroy_pools(dv);
	creator->destroy_buffer(materialDataBuffer);

	for (auto &[k, v] : meshes) {
		creator->destroy_buffer(v->meshBuffers.indexBuffer);
		creator->destroy_buffer(v->meshBuffers.vertexBuffer);
	}

	for (auto &[k, v] : images) {
		if (v.image == creator->_errorCheckerboardImage.image) {
			//dont destroy the default images
			continue;
		}

		creator->destroy_image(v);
	}

	for (auto &sampler : samplers) {
		vkDestroySampler(dv, sampler, nullptr);
	}
}
