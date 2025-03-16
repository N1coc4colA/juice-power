#include "camera.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>


void Camera::update()
{
	const glm::mat4 cameraRotation = getRotationMatrix();
	position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}

void Camera::processSDLEvent(SDL_Event &e)
{
	// [TODO] Replace with switches
	switch (e.type) {
		case SDL_EVENT_KEY_DOWN: {
			switch (e.key.scancode) {
				case SDL_SCANCODE_W: {
					velocity.z = -1.f;
				}
				case SDL_SCANCODE_S: {
					velocity.z = 1.f;
				}
				case SDL_SCANCODE_A: {
					velocity.x = -1.f;
				}
				case SDL_SCANCODE_D: {
					velocity.x = 1.f;
				}
				default: break;
			}
		}
		case SDL_EVENT_KEY_UP: {
			switch (e.key.scancode) {
				case SDL_SCANCODE_W: {
					velocity.z = 0.f;
				}
				case SDL_SCANCODE_S: {
					velocity.z = 0.f;
				}
				case SDL_SCANCODE_A: {
					velocity.x = 0.f;
				}
				case SDL_SCANCODE_D: {
					velocity.x = 0.f;
				}
				default: break;
			}
		}
		case SDL_EVENT_MOUSE_MOTION: {
			yaw += (float)e.motion.xrel / 200.f;
			pitch -= (float)e.motion.yrel / 200.f;
		}
		default: break;
	}
}

glm::mat4 Camera::getViewMatrix()
{
	// to create a correct model view, we need to move the world in opposite
	// direction to the camera
	//  so we will create the camera model matrix and invert
	const glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
	const glm::mat4 cameraRotation = getRotationMatrix();
	return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix()
{
	// fairly typical FPS style camera. we join the pitch and yaw rotations into
	// the final rotation matrix

	const glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3 { 1.f, 0.f, 0.f });
	const glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3 { 0.f, -1.f, 0.f });

	return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}
