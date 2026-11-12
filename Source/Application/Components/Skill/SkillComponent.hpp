#pragma once
#include"entt/entt.hpp"

#include<vector>
#include<array>

namespace Engine::System
{
	/// <summary>
	/// スキルコンポーネント
	/// インスタンスは破棄せずにタグでアクティブかどうかを切り替えます。
	/// </summary>
	struct SkillComponent
	{
		//	有効な時間
		float CooldownTimer = 0.0f;
		//	クールタイム
		float MaxCooldown = 5.0f;
		//	エフェクトの種類
		enum class eKind { Wind, Fire };
		eKind Kind = eKind::Wind;
		//	効果範囲(半径)
		float Radius = 10.0f;
		//	HUDに出す名前
		const char* DisplayName = "";
	};

	/// <summary>
	/// アクティブなスキルのタグ
	/// </summary>
	struct ActiveSkillTag {
		float ElapsedTime = 0.0f; // 発動してから何秒経ったか
		float Duration = 2.0f;    // 何秒間アクティブでいるか
	};

	/// <summary>
	/// プレイヤーにアタッチするスキルスロットコンポーネント
	/// </summary>
	struct SkillSlotComponent
	{
		//	保持しているスキル
		std::array<entt::entity, 2> SkillSlots = { entt::null };
	};
}