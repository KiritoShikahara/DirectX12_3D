#pragma once

#include"System/Entity/System/Base/ISystem.hpp"

namespace Engine::System
{
	class InteractableSystem : public ISystem
	{
	public:
		/// <summary>
		/// 遷移判定
		/// </summary>
		/// <param name="Reg"></param>
		/// <param name="DeltaTime"></param>
		virtual void MainUpdate(entt::registry& Reg, double DeltaTime) override;
		/// <summary>
		/// 判定用の円の描画
		/// </summary>
		/// <param name="Reg"></param>
		virtual void Render(entt::registry& Reg) override;

	private:

	};
}

