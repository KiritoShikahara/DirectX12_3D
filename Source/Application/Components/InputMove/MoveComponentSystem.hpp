#pragma once
#include<Plugin/entt/entt.hpp>

#include"System/Entity/System/Base/ISystem.hpp"

namespace Engine::System
{
	class MoveComponentSystem : public ISystem
	{
	public:
		MoveComponentSystem() = default;

		/// <summary>
		/// 移動量の計算処理をしてRigidの移動量に加算する
		/// </summary>
		virtual void MainUpdate(entt::registry& Reg, double DeltaTime) override;
		virtual void Render(entt::registry& Reg) override;
	private:

	};
}

