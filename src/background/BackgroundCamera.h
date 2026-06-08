#pragma once

#include <glm/glm.hpp>

namespace ocean_bg {

class BackgroundCamera {
public:
  explicit BackgroundCamera(
      const glm::vec3 &position = glm::vec3(0.0f, 1.2f, 6.0f));

  glm::mat4 viewMatrix() const;
  const glm::vec3 &position() const;
  float fov() const;
  void addYawOffset(float deltaDeg);
  void addMovement(float forwardDelta, float strafeDelta);
  void resetInteractiveControls();

private:
  void updateVectors();

  glm::vec3 basePosition_;
  glm::vec3 position_;
  glm::vec3 front_;
  glm::vec3 up_;
  glm::vec3 right_;
  const glm::vec3 worldUp_ = glm::vec3(0.0f, 1.0f, 0.0f);

  float yawOffsetDeg_ = 0.0f;
  float forwardOffset_ = 0.0f;
  float strafeOffset_ = 0.0f;
  const float baseYaw_ = -90.0f;
  const float basePitch_ = -8.53f;
  float fov_ = 75.0f;
};

} // namespace ocean_bg
