#include "render/orbit_camera.hpp"

#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

namespace LeagueModel
{
	void OrbitCamera::SetTarget(const glm::vec3& target)
	{
		m_target = target;
	}

	void OrbitCamera::SetDistance(float distance)
	{
		m_distance = std::clamp(distance, 75.0f, 1200.0f);
	}

	void OrbitCamera::OnRotate(float deltaX, float deltaY)
	{
		m_yaw -= deltaX * 0.01f;
		m_pitch -= deltaY * 0.01f;
		m_pitch = std::clamp(m_pitch, -1.3f, 1.3f);
	}

	void OrbitCamera::OnZoom(float delta)
	{
		SetDistance(m_distance - delta * 20.0f);
	}

	glm::vec3 OrbitCamera::GetPosition() const
	{
		glm::vec3 offset = glm::vec3(0.0f, 0.0f, m_distance);
		offset = glm::rotateX(offset, m_pitch);
		offset = glm::rotateY(offset, m_yaw);
		return m_target + offset;
	}

	glm::mat4 OrbitCamera::GetViewMatrix() const
	{
		return glm::lookAt(GetPosition(), m_target, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::mat4 OrbitCamera::GetProjectionMatrix(float aspectRatio) const
	{
		const float safeAspect = aspectRatio > 0.0f ? aspectRatio : 1.0f;
		return glm::perspective(m_fovRadians, safeAspect, 1.0f, 5000.0f);
	}
}
