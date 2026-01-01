#pragma once

#include "PlatformTypes.h"

#define GLM_FORCE_INTRINSICS
#define GLM_GTX_hash
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

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

template <typename T, glm::qualifier Q>
GLM_FUNC_DECL GLM_CONSTEXPR bool operator>=(glm::vec<3, T, Q> const& v1, glm::vec<3, T, Q> const& v2)
{
	return v1.x >= v2.x && v1.y >= v2.y && v1.z >= v2.z;
}

template <typename T, glm::qualifier Q>
GLM_FUNC_DECL GLM_CONSTEXPR bool operator>(glm::vec<3, T, Q> const& v1, glm::vec<3, T, Q> const& v2)
{
	return v1.x > v2.x && v1.y > v2.y && v1.z > v2.z;
}

template <typename T, glm::qualifier Q>
GLM_FUNC_DECL GLM_CONSTEXPR bool operator<=(glm::vec<3, T, Q> const& v1, glm::vec<3, T, Q> const& v2)
{
	return v1.x <= v2.x && v1.y <= v2.y && v1.z <= v2.z;
}

template <typename T, glm::qualifier Q>
GLM_FUNC_DECL GLM_CONSTEXPR bool operator<(glm::vec<3, T, Q> const& v1, glm::vec<3, T, Q> const& v2)
{
	return v1.x < v2.x && v1.y < v2.y && v1.z < v2.z;
}

namespace Math
{
	struct FRect
	{
		glm::vec2 Min;
		glm::vec2 Max;
	};

	inline auto RectFromMinMax(glm::vec2 Min, glm::vec2 Max) -> FRect
	{
		return FRect{ .Min = Min, .Max = Max };
	}
	inline auto RectFromCenter(glm::vec2 Center, glm::vec2 HalfDim) -> FRect
	{
		return FRect{ .Min = Center - HalfDim, .Max = Center + HalfDim };
	}
	inline auto RectFromLeftTop(glm::vec2 LeftTop, glm::vec2 Dim) -> FRect
	{
		return FRect{ .Min = LeftTop, .Max = LeftTop + Dim };
	}

	inline auto IsInRect(FRect Rect, glm::vec2 Test) -> bool
	{
		return (Test >= Rect.Min) && (Test < Rect.Max);
	}

	struct FCubeInt
	{
		glm::ivec3 Min;
		glm::ivec3 Max;
	};

	inline auto RectFromMinMax(glm::ivec3 Min, glm::ivec3 Max) -> FCubeInt
	{
		return FCubeInt{ .Min = Min, .Max = Max };
	}
	inline auto RectFromCenter(glm::ivec3 Center, glm::ivec3 HalfDim) -> FCubeInt
	{
		return FCubeInt{ .Min = Center - HalfDim, .Max = Center + HalfDim };
	}
	inline auto RectFromLeftTop(glm::ivec3 LeftTop, glm::ivec3 Dim) -> FCubeInt
	{
		return FCubeInt{ .Min = LeftTop, .Max = LeftTop + Dim };
	}
	inline auto IsInRect(FCubeInt Rect, glm::ivec3 Test) -> bool
	{
		return (Test >= Rect.Min) && (Test < Rect.Max);
	}
} // namespace Math
