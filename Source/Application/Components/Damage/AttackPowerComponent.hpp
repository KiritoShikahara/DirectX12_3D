#pragma once

namespace Engine::System
{
	/// <summary>
	/// 攻撃力(武器・スキル)。ヒット時の演出は DamageSystem が決める
	/// </summary>
	struct AttackPowerComponent
	{
		float DamageValue = 10;
	};
}
