#pragma once
#include"Math/Vector3/Vector3.h"
#include"Graphics/Color/Color.hpp"

// =====================================================================
//  ゲーム用エフェクトのプリセット (自作パーティクル)
//  Document/GameDesign.md の「エフェクト」と対応
// =====================================================================
namespace Engine::Vfx
{
	/// <summary>
	/// 斬撃ヒットの火花。Direction は攻撃の向き
	/// </summary>
	void HitSpark(const Math::Vector3& Position, const Math::Vector3& Direction, const Graphics::Color& Tint, float Scale = 1.0f);

	/// <summary>
	/// ジャストパリィ(青白い大閃光)
	/// </summary>
	void ParryFlash(const Math::Vector3& Position);

	/// <summary>
	/// ガード(小さな橙の火花)
	/// </summary>
	void GuardSpark(const Math::Vector3& Position);

	/// <summary>
	/// 敵攻撃の予告閃光。Parryable=false なら赤
	/// </summary>
	void Telegraph(const Math::Vector3& Position, bool Parryable);

	/// <summary>
	/// 回避の砂煙
	/// </summary>
	void DodgeDust(const Math::Vector3& FootPosition);

	/// <summary>
	/// 体幹崩し
	/// </summary>
	void BreakBurst(const Math::Vector3& Position);

	/// <summary>
	/// 撃破
	/// </summary>
	void DeathBurst(const Math::Vector3& Position, const Graphics::Color& Tint);

	/// <summary>
	/// 風のスキル(竜巻)
	/// </summary>
	void SkillWind(const Math::Vector3& Position, float Radius);

	/// <summary>
	/// 炎のスキル(火柱と火の粉)
	/// </summary>
	void SkillFire(const Math::Vector3& Position, float Radius);

	/// <summary>
	/// ガード中の守りの光(毎フレーム呼ぶ)
	/// </summary>
	void GuardAura(const Math::Vector3& Position);

	/// <summary>
	/// 崩れている敵の頭上の星(毎フレーム呼ぶ)
	/// </summary>
	void StunSparkle(const Math::Vector3& HeadPosition);
}
