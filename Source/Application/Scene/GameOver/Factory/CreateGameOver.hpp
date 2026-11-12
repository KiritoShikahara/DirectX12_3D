#pragma once
#include"entt/entt.hpp"

namespace Engine::Scene
{
	class GameOverFactory
	{
	public:
		//	背景
		static void CreateBG();

		//	Titleボタン
		static entt::entity CreateTitle();

		//	Retryボタン
		static entt::entity CreateRetry();

	};

}