#pragma once

#include <Platform.h>

namespace Memory::Arena
{
	struct FMemoryArena
	{
		usize Size;
		void* Memory;
		usize Used;
	};

	auto Initialize(FMemoryArena& Arena, usize Size, void* Memory) -> void
	{
		Arena.Size = Size;
		Arena.Memory = Memory;
		Arena.Used = 0;
	}

	auto Allocate(FMemoryArena& Arena, usize Size) -> void*
	{
		if (Arena.Used + Size <= Arena.Size)
		{
			void* Result = (u8*)Arena.Memory + Arena.Used;
			Arena.Used += Size;
			return Result;
		}

		return nullptr;
	}

	template <typename AllocType>
	auto Allocate(FMemoryArena& Arena) -> AllocType*
	{
		usize Size = sizeof(AllocType);
		return (AllocType*)Allocate(Arena, Size);
	}

	template <typename AllocType>
	auto AllocateArray(FMemoryArena& Arena, usize Count) -> AllocType*
	{
		usize Size = sizeof(AllocType) * Count;
		return (AllocType*)Allocate(Arena, Size);
	}
} // namespace Memory::Arena