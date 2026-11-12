#pragma once
#include"System/Entity/System/Base/ISystem.hpp"
#include"Application/Components/HpRender/HpRenderComponent.hpp"


namespace Engine::System
{
	class HpRenderSystem : public ISystem
	{
	public:
		virtual void PostUpdate(entt::registry& Reg, double DeltaTime)override;
		virtual void Render(entt::registry& Reg) override;
	private:
		/// <summary>
		/// 体力の残量で色を変更する
		/// </summary>
		void UpdateColor(HpRenderComponent& Component,float Ratio);

		/// <summary>
		/// GUIで調整できるように
		/// </summary>
		/// <param name="Reg"></param>
		void OnGui(entt::registry& Reg);
	};
}


