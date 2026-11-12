#pragma once
#include"System/Entity/System/Base/ISystem.hpp"

namespace Engine::System
{
	class DamageSystem : public ISystem
	{
	public:
		void MainUpdate(entt::registry& Reg, double DeltaTime) {}

		/// <summary>
		/// ダメージ処理
		/// </summary>
		/// <param name="Reg"></param>
		/// <param name="DeltaTime"></param>
		void PostUpdate(entt::registry& Reg, double DeltaTime)override;
	private:


	};
}