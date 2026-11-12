#pragma once
#include<vector>
#include<Plugin/entt/entt.hpp>

namespace Engine::System
{
	struct CollisionEvent
	{
		/// <summary>
		/// 当たってきた相手のコレクション
		/// </summary>
		std::vector<entt::entity> HitEntitys;
	};
}