#include "pch.h"
#include "CombatRules.hpp"
#include <algorithm>

namespace Engine::Combat
{
	DefenseResult ResolveDefense(const DefenseInput& Input, bool AttackParryable, float Stamina)
	{
		const bool inParryWindow = Input.TimeSincePress <= Tuning::ParryWindow && Input.ParryLocked == false;
		if (AttackParryable && inParryWindow)
		{
			return DefenseResult::Parried;
		}
		if (Input.Holding)
		{
			return (Stamina >= Tuning::GuardStaminaCost) ? DefenseResult::Guarded : DefenseResult::GuardBroken;
		}
		return DefenseResult::Hit;
	}

	float DamageAfterDefense(DefenseResult Result, float BaseDamage)
	{
		switch (Result)
		{
		case DefenseResult::Parried:
			return 0.0f;
		case DefenseResult::Guarded:
			return BaseDamage * Tuning::GuardDamageRate;
		case DefenseResult::GuardBroken:
			return BaseDamage * 0.5f;
		case DefenseResult::Hit:
		default:
			return BaseDamage;
		}
	}

	bool AddPosture(Posture& Target, float Amount)
	{
		Target.NoHitTimer = 0.0f;
		if (Target.IsBroken())
		{
			return false;
		}
		Target.Value = (std::min)(Target.Max, Target.Value + Amount);
		if (Target.Value >= Target.Max)
		{
			Target.BrokenTimer = Tuning::BreakDuration;
			return true;
		}
		return false;
	}

	void UpdatePosture(Posture& Target, float DeltaTime)
	{
		if (Target.IsBroken())
		{
			Target.BrokenTimer -= DeltaTime;
			if (Target.BrokenTimer <= 0.0f)
			{
				//	崩れ終わったらゲージは空から
				Target.BrokenTimer = 0.0f;
				Target.Value = 0.0f;
			}
			return;
		}
		Target.NoHitTimer += DeltaTime;
		if (Target.NoHitTimer >= Tuning::PostureRecoverDelay)
		{
			Target.Value = (std::max)(0.0f, Target.Value - Tuning::PostureRecoverPerSecond * DeltaTime);
		}
	}

	bool TryConsumeStamina(Stamina& Target, float Cost)
	{
		if (Target.Value < Cost)
		{
			return false;
		}
		Target.Value -= Cost;
		Target.RegenDelayTimer = Tuning::StaminaRegenDelay;
		return true;
	}

	void ForceConsumeStamina(Stamina& Target, float Cost)
	{
		Target.Value = (std::max)(0.0f, Target.Value - Cost);
		Target.RegenDelayTimer = Tuning::StaminaRegenDelay;
	}

	void UpdateStamina(Stamina& Target, float DeltaTime)
	{
		if (Target.RegenDelayTimer > 0.0f)
		{
			Target.RegenDelayTimer -= DeltaTime;
			return;
		}
		Target.Value = (std::min)(Target.Max, Target.Value + Tuning::StaminaRegenPerSecond * DeltaTime);
	}

	AttackPhase GetAttackPhase(float Progress)
	{
		if (Progress < Tuning::ActiveBegin)
		{
			return AttackPhase::Windup;
		}
		if (Progress < Tuning::ActiveEnd)
		{
			return AttackPhase::Active;
		}
		return AttackPhase::Recovery;
	}

	float DamageMultiplier(const Posture& Target)
	{
		return Target.IsBroken() ? Tuning::BreakDamageRate : 1.0f;
	}
}
