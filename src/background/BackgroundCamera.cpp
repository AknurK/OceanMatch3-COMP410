#include "background/BackgroundCamera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace ocean_bg {

BackgroundCamera::BackgroundCamera(const glm::vec3 &position)
    : basePosition_(position), position_(position) {
  updateVectors();
}

glm::mat4 BackgroundCamera::viewMatrix() const {
  return glm::lookAt(position_, position_ + front_, up_);
}

const glm::vec3 &BackgroundCamera::position() const { return position_; }

float BackgroundCamera::fov() const { return fov_; }

void BackgroundCamera::addYawOffset(float deltaDeg) {
  yawOffsetDeg_ = std::clamp(yawOffsetDeg_ + deltaDeg, -16.0f, 16.0f);
  updateVectors();
}

void BackgroundCamera::addMovement(float forwardDelta, float strafeDelta) {
  forwardOffset_ = std::clamp(forwardOffset_ + forwardDelta, -1.1f, 1.6f);
  strafeOffset_ = std::clamp(strafeOffset_ + strafeDelta, -0.8f, 0.8f);
  updateVectors();
}

void BackgroundCamera::resetInteractiveControls() {
  yawOffsetDeg_ = 0.0f;
  forwardOffset_ = 0.0f;
  strafeOffset_ = 0.0f;
  updateVectors();
}

void BackgroundCamera::updateVectors() {
  const float yaw = glm::radians(baseYaw_ + yawOffsetDeg_);
  const float pitch = glm::radians(basePitch_);
  front_ = glm::normalize(glm::vec3(std::cos(yaw) * std::cos(pitch),
                                    std::sin(pitch),
                                    std::sin(yaw) * std::cos(pitch)));
  right_ = glm::normalize(glm::cross(front_, worldUp_));
  up_ = glm::normalize(glm::cross(right_, front_));

  glm::vec3 flatFront(front_.x, 0.0f, front_.z);
  if (glm::length(flatFront) > 0.0001f)
    flatFront = glm::normalize(flatFront);
  position_ = basePosition_ + flatFront * forwardOffset_ +
              right_ * strafeOffset_;
}

} // namespace ocean_bg
