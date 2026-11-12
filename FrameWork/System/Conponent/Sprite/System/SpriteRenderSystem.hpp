#pragma once
#include"../SpriteComponent.hpp"


namespace Engine::System
{
	class SpriteRenderSystem
	{
	public:
		/// <summary>
		/// 座標の同期
		/// </summary>
		/// <param name="Reg"></param>
		static void Update(entt::registry& Reg);

		/// <summary>
		/// 描画
		/// </summary>
		/// <param name="Reg"></param>
		static void Render(entt::registry& Reg);
	};
}

