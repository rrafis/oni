#pragma once

#include "PlatformTypes.h"

#define GLM_FORCE_INTRINSICS
#include <glm/glm.hpp>

static_assert(sizeof(glm::uvec1) == 4);
static_assert(sizeof(glm::uvec2) == 8);
static_assert(sizeof(glm::uvec3) == 12);
static_assert(sizeof(glm::uvec4) == 16);

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
