#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace LeagueModel
{
	class OrbitCamera
	{
	public:
		void SetTarget(const glm::vec3& target);
		void SetDistance(float distance);

		void OnRotate(float deltaX, float deltaY);
		void OnZoom(float delta);

		glm::vec3 GetPosition() const;
		glm::mat4 GetViewMatrix() const;
		glm::mat4 GetProjectionMatrix(float aspectRatio) const;

	private:
		glm::vec3 m_target = glm::vec3(0.0f);
		float m_yaw = 0.0f;
		float m_pitch = 0.35f;
		float m_distance = 400.0f;
		float m_fovRadians = 0.78539816339f;
	};
}
