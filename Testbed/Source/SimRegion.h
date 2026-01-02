#pragma once

#include "Components.h"
#include "GameMath.h"
#include "World.h"

namespace Engine
{
	struct FSimRegion
	{
		FWorld* World;
		entt::registry* Registry;
		FWorldPosition Position;
		Math::FCubeInt Bounds;
	};

	auto BeginSimRegion(entt::registry& Registry, FWorld& World, FWorldPosition RegionPos, Math::FCubeInt RegionBounds) -> FSimRegion
	{
		for (i32 Z = RegionBounds.Min.z; Z < RegionBounds.Max.z; ++Z)
		{
			for (i32 Y = RegionBounds.Min.y; Y < RegionBounds.Max.y; ++Y)
			{
				for (i32 X = RegionBounds.Min.x; X < RegionBounds.Max.x; ++X)
				{
					if (auto ChunkStorage = Engine::GetChunkStorage(World, glm::ivec3(X, Y, Z)))
					{
						for (auto& Entity : *ChunkStorage)
						{
							auto& ActiveTag = Entity.get_or_emplace<Comp::FActiveTag>();
							ActiveTag.Chunk = glm::ivec3(X, Y, Z);
						}
					}
				}
			}
		}

		auto View = Registry.view<Comp::FTransform, Comp::FWorldTransform, Comp::FActiveTag>();
		for (auto [Entity, Transform, WorldTransform, ActiveTag] : View.each())
		{
			Engine::MapToCameraSpace(World, RegionPos, WorldTransform.Position, Transform.Position);
		}

		return FSimRegion{ .World = &World, .Registry = &Registry, .Position = RegionPos, .Bounds = RegionBounds };
	}

	auto EndSimRegion(FSimRegion Region)
	{
		if (Region.World && Region.Registry)
		{
			auto View = Region.Registry->view<Comp::FTransform, Comp::FWorldTransform, Comp::FActiveTag>();
			for (auto [Entity, Transform, WorldTransform, ActiveTag] : View.each())
			{
				auto OldChunk = WorldTransform.Position.Chunk;
				Engine::MapToChunkSpace(*Region.World, Region.Position, Transform.Position, WorldTransform.Position);
				Engine::ChangeLocation(*Region.World, entt::handle(*Region.Registry, Entity), OldChunk, WorldTransform.Position.Chunk);
				Region.Registry->remove<Comp::FActiveTag>(Entity);
			}

			for (i32 Z = Region.Bounds.Min.z; Z < Region.Bounds.Max.z; ++Z)
			{
				for (i32 Y = Region.Bounds.Min.y; Y < Region.Bounds.Max.y; ++Y)
				{
					for (i32 X = Region.Bounds.Min.x; X < Region.Bounds.Max.x; ++X)
					{
						if (auto ChunkStorage = Engine::GetChunkStorage(*Region.World, glm::ivec3(X, Y, Z)))
						{
							for (auto& Entity : *ChunkStorage)
							{
								if (Entity.any_of<Comp::FActiveTag>())
									Entity.remove<Comp::FActiveTag>();
							}
						}
					}
				}
			}
		}
	}
} // namespace Engine
