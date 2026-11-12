#pragma once
#include"System/Entity/System/Base/ISystem.hpp"
#include"Math/Vector3/Vector3.h"

namespace Engine::System
{
	struct Transform3D;

	/// <summary>
	/// 戦闘の進行(防御入力・スタミナ・体幹・敵の攻撃フェーズ・よろめき・武器の軌跡)
	/// </summary>
	class CombatSystem : public ISystem
	{
	public:
		void PreUpdate(entt::registry& Reg, double DeltaTime)override;
		void MainUpdate(entt::registry& Reg, double DeltaTime)override;
		void PostUpdate(entt::registry& Reg, double DeltaTime)override;

		/// <summary>
		/// 敵をよろめかせる(攻撃を中断してAIを止める)
		/// </summary>
		static void BeginStagger(entt::registry& Reg, entt::entity Enemy, float Seconds);

		/// <summary>
		/// 武器のローカルY位置をワールド座標へ
		/// </summary>
		static Math::Vector3 WeaponPoint(const Transform3D& WeaponTransform, float LocalY);

	private:
		void UpdateEnemyAttacks(entt::registry& Reg, float DeltaTime);
		void UpdateStagger(entt::registry& Reg, float DeltaTime);
	};
}
