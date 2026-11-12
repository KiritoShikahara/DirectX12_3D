#pragma once
#include"Application/Combat/CombatRules.hpp"
#include"Graphics/Color/Color.hpp"
#include"Graphics/Particles/ParticleSystem.hpp"
#include<string>
#include<Plugin/entt/entt.hpp>

namespace Engine::System
{
	/// <summary>
	/// スタミナ(プレイヤー)
	/// </summary>
	struct StaminaComponent
	{
		Combat::Stamina Value;
		float DeniedFlashTimer = 0.0f;	//	不足時にゲージを赤く点滅させる
	};

	/// <summary>
	/// 防御(パリィ/ガード)の入力状態
	/// </summary>
	struct DefenseComponent
	{
		bool Holding = false;
		float TimeSincePress = 1e9f;
		float LockTimer = 0.0f;		//	連打防止
		float StunTimer = 0.0f;		//	ガード崩しで動けない時間
		float DamageFlashTimer = 0.0f;	//	被弾時の画面演出

		Combat::DefenseInput ToInput()const
		{
			Combat::DefenseInput input;
			input.Holding = Holding;
			input.TimeSincePress = TimeSincePress;
			return input;
		}
	};

	/// <summary>
	/// 体幹(敵)
	/// </summary>
	struct PostureComponent
	{
		Combat::Posture Value;
	};

	/// <summary>
	/// 敵の戦闘用データ(攻撃フェーズ・よろめき・表示名)
	/// </summary>
	struct EnemyCombatComponent
	{
		std::string DisplayName;
		Graphics::Color WeaponColor = { 1, 1, 1, 1 };
		float UnparryableChance = 0.0f;		//	弾けない攻撃の確率
		float AttackDamageScale = 1.0f;

		//	今の攻撃の状態
		bool Parryable = true;
		bool TelegraphFired = false;
		bool AttackActive = false;
		float LastProgress = 0.0f;

		float StaggerTimer = 0.0f;	//	パリィされたよろめき
		float HitFlashTimer = 0.0f;	//	被弾時の白フラッシュ
		bool IsTrainingDummy = false;	//	チュートリアル用(倒されない・弾ける攻撃だけ)
	};

	/// <summary>
	/// よろめき/体幹崩し中(AIを止める)
	/// </summary>
	struct StaggeredTag {};

	/// <summary>
	/// チュートリアルの木人を倒せなくする(体力1で止める)
	/// </summary>
	struct DummyInvulnerableTag {};

	/// <summary>
	/// 攻撃してこない敵(チュートリアル前半)
	/// </summary>
	struct EnemyPassiveTag {};

	/// <summary>
	/// 武器の軌跡
	/// </summary>
	struct WeaponTrailComponent
	{
		Graphics::TrailHandle Trail;
		entt::entity Owner = entt::null;
		float BaseLocal = -2.0f;	//	刃の根本(武器ローカルY)
		float TipLocal = -5.6f;		//	刃の先端
	};
}
