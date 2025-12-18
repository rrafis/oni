#pragma once

#include <sys/types.h>

// Unsigned int types.

/** @brief Unsigned 8-bit integer */
using u8 = u_int8_t;
/** @brief Unsigned 16-bit integer */
using u16 = u_int16_t;
/** @brief Unsigned 32-bit integer */
using u32 = u_int32_t;
/** @brief Unsigned 64-bit integer */
using u64 = u_int64_t;

// Signed int types.

/** @brief Signed 8-bit integer */
using i8 = int8_t;
/** @brief Signed 16-bit integer */
using i16 = int16_t;
/** @brief Signed 32-bit integer */
using i32 = int32_t;
/** @brief Signed 64-bit integer */
using i64 = int64_t;

// Floating point types

/** @brief 32-bit floating point number */
using f32 = float;
/** @brief 64-bit floating point number */
using f64 = double;

/** @brief Size */
using usize = size_t;

static_assert(sizeof(u8) == 1);
static_assert(sizeof(u16) == 2);
static_assert(sizeof(u32) == 4);
static_assert(sizeof(u64) == 8);

static_assert(sizeof(i8) == 1);
static_assert(sizeof(i16) == 2);
static_assert(sizeof(i32) == 4);
static_assert(sizeof(i64) == 8);

static_assert(sizeof(f32) == 4);
static_assert(sizeof(f64) == 8);

#if defined(RE_EXPORT)
#define RE_API extern "C" __attribute__((visibility("default")))
#else
#define RE_API
#endif
