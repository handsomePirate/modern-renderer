#include "flycam.h"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

void flycamUpdate(SceneCamera &camera, float timeDeltaSeconds) {
  const float movementSpeed = 500.f;
  {
    glm::vec3 movementForce{};

    glm::vec3 cameraForward = camera.orientation * cameraBaseForward;
    glm::vec3 cameraUp = camera.orientation * cameraBaseUp;
    glm::vec3 cameraRight = camera.orientation * cameraBaseRight;

    auto states = SDL_GetKeyboardState(nullptr);

    if (states[SDL_SCANCODE_A] || states[SDL_SCANCODE_LEFT]) {
      movementForce -= cameraRight;
    }
    if (states[SDL_SCANCODE_D] || states[SDL_SCANCODE_RIGHT]) {
      movementForce += cameraRight;
    }
    if (states[SDL_SCANCODE_S] || states[SDL_SCANCODE_DOWN]) {
      movementForce -= cameraForward;
    }
    if (states[SDL_SCANCODE_W] || states[SDL_SCANCODE_UP]) {
      movementForce += cameraForward;
    }
    if (states[SDL_SCANCODE_F]) {
      movementForce -= cameraUp;
    }
    if (states[SDL_SCANCODE_R]) {
      movementForce += cameraUp;
    }

    movementForce = glm::normalize(movementForce);
    if (std::isnan(movementForce.x))
      movementForce = glm::vec3{};

    camera.position += movementForce * movementSpeed * timeDeltaSeconds;
  }

  {
    float x, y;
    auto flags = SDL_GetRelativeMouseState(&x, &y);

    if (flags == 1) {
      const float rotationSpeed = 0.001f;

      auto yaw = glm::angleAxis(x * rotationSpeed, -cameraBaseUp);
      auto pitch = glm::angleAxis(y * rotationSpeed, -cameraBaseRight);

      camera.orientation = yaw * camera.orientation * pitch;
    }
  }
}
