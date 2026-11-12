#pragma once
#include<entt/entt.hpp>

namespace Engine::System
{
	
	/// <summary>
	/// 敵の状態管理
	/// </summary>
	struct EnemyStateComponent
	{

		//	武器
		entt::entity Weapon = entt::null;
	};
}