#pragma once
#include"System/Scene/IScene.hpp"

namespace Engine::Scene
{
	class Title : public Engine::System::IScene
	{
	public:
		/// <summary>
		///	初期化
		/// </summary>
		/// <returns></returns>
		bool Initialize()override;

		/// <summary>
		/// 画面の切り替え判定
		/// </summary>
		/// <param name="FixedDeltaTime"></param>
		void PostUpdate(double FixedDeltaTime)override;

		/// <summary>
		/// 終了処理
		/// </summary>
		void Release()override;
		void Render()override;

		std::string GetSceneName()override
		{
			return "Title";
		}
	private:
		float mTime = 0.0f;

	};
}