#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <SDL3/SDL_events.h>


class Camera
{
public:
	glm::vec3 velocity {};
	glm::vec3 position {};
	// vertical rotation
	float pitch = 0.f;
	// horizontal rotation
	float yaw = 0.f;

	glm::mat4 getViewMatrix() const;
	glm::mat4 getRotationMatrix() const;

	void processSDLEvent(SDL_Event &e);

	void update();
};


#endif // CAMERA_H
