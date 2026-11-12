#pragma once
#include "Application/Objects/EnemyRank.hpp"
#include"entt/entt.hpp"
#include<string>
#include "Graphics/Texture/Texture.hpp"

namespace Engine::System
{
	struct InteractableComponent
	{
		float InteractRange = 10.0f;	//	インタラクトできる距離
		EnemyRank Rank = EnemyRank::Basic;	//	どの状態に遷移するか
		int UnlockLevel = 0;
		bool Talkable = true;			//	離せる状態かどうか
		bool IsTutorial = false;		//	true: チュートリアルへ
		std::string Label;				//	頭上に出す案内(空なら出さない)
	};

}