#pragma once
#include"System/Entity/System/Base/ISystem.hpp"
namespace Engine::System
{
	class GameRuleSystem : public ISystem
	{
	public:

		/// <summary>
		/// Gameの終了条件を見る
		/// </summary>
		/// <param name="Reg"></param>
		/// <param name="DeltaTime"></param>
		void PostUpdate(entt::registry& Reg, double DeltaTime)override;

	private:
		bool mIsStateChange = false;
	};
}


