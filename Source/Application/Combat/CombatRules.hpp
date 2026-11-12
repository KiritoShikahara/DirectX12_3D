#pragma once

// =====================================================================
//  戦闘ルール(純粋関数)。数値は Document/GameDesign.md と対応
//  エンジンに依存しないので単体テスト(Tests/CombatRulesTests.cpp)から直接呼ぶ
// =====================================================================
namespace Engine::Combat
{
	namespace Tuning
	{
		//	パリィ / ガード
		constexpr float ParryWindow = 0.18f;		//	押してからジャストパリィになる時間
		constexpr float ParryLockout = 0.35f;		//	受付終了後、再受付できない時間
		constexpr float GuardDamageRate = 0.25f;	//	ガード時の被ダメージ倍率
		constexpr float GuardStaminaCost = 20.0f;
		constexpr float GuardBreakStun = 0.8f;
		constexpr float ParryPostureDamage = 35.0f;
		constexpr float ParryEnemyStagger = 0.6f;

		//	体幹
		constexpr float PostureMax = 100.0f;
		constexpr float PostureOnHit = 6.0f;
		constexpr float PostureRecoverDelay = 2.0f;
		constexpr float PostureRecoverPerSecond = 10.0f;
		constexpr float BreakDuration = 2.5f;
		constexpr float BreakDamageRate = 2.5f;

		//	スタミナ
		constexpr float StaminaMax = 100.0f;
		constexpr float DodgeStaminaCost = 25.0f;
		constexpr float StaminaRegenPerSecond = 30.0f;
		constexpr float StaminaRegenDelay = 0.6f;

		//	敵の攻撃フェーズ(アニメーション進行率)
		constexpr float TelegraphAt = 0.32f;
		constexpr float ActiveBegin = 0.40f;
		constexpr float ActiveEnd = 0.70f;
	}

	enum class DefenseResult
	{
		Hit,			//	被弾
		Guarded,		//	ガード成功
		GuardBroken,	//	スタミナ切れでガードを崩された
		Parried,		//	ジャストパリィ
	};

	/// <summary>
	/// プレイヤーの防御入力の状態
	/// </summary>
	struct DefenseInput
	{
		bool Holding = false;			//	防御ボタンを押している
		float TimeSincePress = 1e9f;	//	最後に押してからの時間
		bool ParryLocked = false;		//	連打防止で受付不可
	};

	/// <summary>
	/// 攻撃を受けた時の防御結果を決める
	/// </summary>
	DefenseResult ResolveDefense(const DefenseInput& Input, bool AttackParryable, float Stamina);

	/// <summary>
	/// 防御結果から実際のダメージ
	/// </summary>
	float DamageAfterDefense(DefenseResult Result, float BaseDamage);

	struct Posture
	{
		float Value = 0.0f;
		float Max = Tuning::PostureMax;
		float NoHitTimer = 0.0f;
		float BrokenTimer = 0.0f;

		bool IsBroken()const { return BrokenTimer > 0.0f; }
		float Ratio()const { return Max > 0.0f ? Value / Max : 0.0f; }
	};

	/// <summary>
	/// 体幹ダメージを加える
	/// </summary>
	/// <returns>true: この加算で体幹が崩れた</returns>
	bool AddPosture(Posture& Target, float Amount);

	/// <summary>
	/// 体幹の自然回復・崩れ時間の経過
	/// </summary>
	void UpdatePosture(Posture& Target, float DeltaTime);

	struct Stamina
	{
		float Value = Tuning::StaminaMax;
		float Max = Tuning::StaminaMax;
		float RegenDelayTimer = 0.0f;

		float Ratio()const { return Max > 0.0f ? Value / Max : 0.0f; }
	};

	/// <summary>
	/// 足りれば消費する
	/// </summary>
	/// <returns>true: 消費した</returns>
	bool TryConsumeStamina(Stamina& Target, float Cost);

	/// <summary>
	/// 足りなくても消費する(0で止まる)
	/// </summary>
	void ForceConsumeStamina(Stamina& Target, float Cost);

	void UpdateStamina(Stamina& Target, float DeltaTime);

	enum class AttackPhase
	{
		Windup,
		Active,
		Recovery,
	};

	AttackPhase GetAttackPhase(float Progress);

	/// <summary>
	/// 体幹が崩れている相手へのダメージ倍率
	/// </summary>
	float DamageMultiplier(const Posture& Target);
}
