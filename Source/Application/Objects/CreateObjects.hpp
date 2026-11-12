#pragma once
#include<Plugin/entt/entt.hpp>
#include<string>

#include"Graphics/Color/Color.hpp"
#include"Math/Vector3/Vector3.h"
#include"Application/Objects/EnemyRank.hpp"

namespace Engine::System
{
	/// <summary>
	/// 敵の見た目と戦い方の仕様
	/// </summary>
	struct EnemySpec
	{
		int DataId = 1;						//	EnemyData.csv の行
		EnemyRank Rank = EnemyRank::Basic;
		const char* DisplayName = "";
		Graphics::Color Tint = { 1, 1, 1, 1 };
		Math::Vector3 SpawnPosition = { 20.0f, 0.0f, 20.0f };
		float UnparryableChance = 0.0f;		//	弾けない攻撃の確率
		float AttackAnimScale = 0.7f;		//	攻撃の速さ
		float PostureMax = 100.0f;
		float Metallic = 0.6f;				//	鎧の質感
		bool TrainingDummy = false;
	};

	class ObjectsFactory
	{
		static entt::entity CreatePlayerWeapon(entt::entity Parent,const std::string& BoneName,int Damage);
		static entt::entity CreateEnemyWeapon(entt::entity Parent, const std::string& BoneName,int Damage,const Graphics::Color& Color);

		static entt::entity CreateSkill1();
		static entt::entity CreateSkill2();

	public:
		static entt::entity CreatePlayer();
		static entt::entity CreateEnemy(const EnemySpec& Spec);
		static void CreateEnemy_Basic();     // 青（最初）
		static void CreateEnemy_Advanced();  // 黄（中盤）
		static void CreateEnemy_Boss();      // 赤（最強）
		static entt::entity CreateTrainingDummy();	//	チュートリアルの木人
		static void CreateField();
	};
}
