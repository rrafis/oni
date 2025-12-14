#pragma once

#define GLM_FORCE_INTRINSICS
#include <glm/glm.hpp>

template <typename T, glm::qualifier Q>
GLM_FUNC_DECL GLM_CONSTEXPR bool operator>=(glm::vec<2, T, Q> const& v1, glm::vec<2, T, Q> const& v2)
{
	return v1.x >= v2.x && v1.y >= v2.y;
}

template <typename T, glm::qualifier Q>
GLM_FUNC_DECL GLM_CONSTEXPR bool operator>(glm::vec<2, T, Q> const& v1, glm::vec<2, T, Q> const& v2)
{
	return v1.x > v2.x && v1.y > v2.y;
}

template <typename T, glm::qualifier Q>
GLM_FUNC_DECL GLM_CONSTEXPR bool operator<=(glm::vec<2, T, Q> const& v1, glm::vec<2, T, Q> const& v2)
{
	return v1.x <= v2.x && v1.y <= v2.y;
}

template <typename T, glm::qualifier Q>
GLM_FUNC_DECL GLM_CONSTEXPR bool operator<(glm::vec<2, T, Q> const& v1, glm::vec<2, T, Q> const& v2)
{
	return v1.x < v2.x && v1.y < v2.y;
}