#include "pch.h"
#include "SpriteRenderSystem.hpp"

#include "System/Conponent/Transform/TransformConponent.hpp"


namespace Engine::System
{
	/// <summary>
	/// 座標の同期
	/// </summary>
	/// <param name="Reg"></param>
	void SpriteRenderSystem::Update(entt::registry& Reg)
	{
		auto view = Reg.view<Transform2D, SpriteComponent>();
		view.each([&](Transform2D& trans, SpriteComponent&sprite)
			{
				sprite.Sprite.SetPosition(trans.Position);
				sprite.Sprite.SetScale(trans.Scale);
				sprite.Sprite.SetAngle(trans.Rotation);
			});
	}

	/// <summary>
	/// 描画
	/// </summary>
	/// <param name="Reg"></param>
	void SpriteRenderSystem::Render(entt::registry& Reg)
	{
		auto view = Reg.view<SpriteComponent>();
		view.each([&](SpriteComponent& sprite) 
			{
				sprite.Sprite.Render();
			});
	}

}


