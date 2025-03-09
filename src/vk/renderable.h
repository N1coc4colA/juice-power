#ifndef RENDERABLE_H
#define RENDERABLE_H

#include <glm/fwd.hpp>


struct DrawContext;


class Renderable
{
public:
	virtual void draw(const glm::mat4 &topMatrix, DrawContext &ctx) = 0;
};


#endif // RENDERABLE_H
