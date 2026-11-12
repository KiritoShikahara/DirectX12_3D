#pragma once
#include"Math/Vector3/Vector3.h"
#include<vector>

namespace Engine::Combat
{
	enum class CombatEventType
	{
		DamageToEnemy,	//	Value: ダメージ
		DamageToPlayer,	//	Value: ダメージ
		Parry,
		Guard,
		GuardBreak,
		PostureBreak,
		EnemyKilled,
		Dodge,
		DodgeDenied,	//	スタミナ不足
		SkillUsed,		//	Extra: スロット番号
		PlayerAttackHit,
		Telegraph,		//	Extra: 1=弾ける 0=弾けない
	};

	struct CombatEvent
	{
		CombatEventType Type;
		Math::Vector3 Position;
		float Value = 0.0f;
		int Extra = 0;
	};

	/// <summary>
	/// 1フレーム分の戦闘イベント(HUD・チュートリアル・効果音が読む)
	/// フレームの先頭で Application が Clear する
	/// </summary>
	class CombatEvents
	{
	public:
		static void Push(CombatEventType Type, const Math::Vector3& Position, float Value = 0.0f, int Extra = 0)
		{
			sEvents.push_back({ Type, Position, Value, Extra });
		}
		static const std::vector<CombatEvent>& Get() { return sEvents; }
		static void Clear() { sEvents.clear(); }

		static int Count(CombatEventType Type)
		{
			int count = 0;
			for (const auto& e : sEvents)
			{
				if (e.Type == Type) count++;
			}
			return count;
		}

	private:
		static inline std::vector<CombatEvent> sEvents;
	};
}
