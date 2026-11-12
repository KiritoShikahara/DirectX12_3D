#include "pch.h"
#include "DamageSystem.hpp"
#include "AttackPowerComponent.hpp"
#include"Application/Components/Status/StatusComponent.hpp"
#include"System/Collider/CollisionEvents/CollisionEvent.hpp"
#include"Application/Components/Tag/TagComponent.hpp"
#include"Application/Components/WeaponAttack/WeaponAttackComponent.hpp"
#include"Application/Components/Invincible/InvincibleTag.hpp"
#include"Application/Components/Socket/SocketComponent.hpp"
#include"Application/Components/Player/PlayerState/PlayerStateComponent.hpp"
#include"Application/Macro/ProjMacros.hpp"
#include"System/Conponent/Transform/TransformConponent.hpp"

#include"Application/Combat/CombatComponents.hpp"
#include"Application/Combat/CombatEvents.hpp"
#include"Application/Combat/CombatSystem.hpp"
#include"Application/Combat/GameFeel.hpp"
#include"Application/Effects/VfxPresets.hpp"

#include"Audio/Manager/AudioManager.hpp"

namespace
{
	using namespace Engine;
	using namespace Engine::System;
	using Combat::CombatEvents;
	using Combat::CombatEventType;
	using Combat::GameFeel;

	const Graphics::Color PlayerSparkColor = { 0.55f, 0.75f, 1.0f, 1.0f };
	const Graphics::Color PlayerHurtColor = { 1.0f, 0.25f, 0.15f, 1.0f };

	void PlaySE(const char* Path, float Volume)
	{
		Audio::AudioManager::GetInstance()->PlaySE(Path, false, Volume);
	}

	entt::entity FindPlayer(entt::registry& Reg)
	{
		auto view = Reg.view<PlayerTag>();
		return view.empty() ? entt::null : view.front();
	}

	/// <summary>
	/// 体幹を削り、崩れたら演出してスタンさせる
	/// </summary>
	void ApplyPosture(entt::registry& Reg, entt::entity Enemy, float Amount, const Math::Vector3& EffectPos)
	{
		auto* posture = Reg.try_get<PostureComponent>(Enemy);
		if (posture == nullptr)
		{
			return;
		}
		if (Combat::AddPosture(posture->Value, Amount))
		{
			Vfx::BreakBurst(EffectPos);
			GameFeel::HitStop(0.15f);
			GameFeel::SlowMotion(0.25f, 0.6f);
			GameFeel::Shake(0.8f, 0.4f);
			CombatEvents::Push(CombatEventType::PostureBreak, EffectPos);
			PlaySE("Assets/Sound/Sword.aud", 0.9f);
			CombatSystem::BeginStagger(Reg, Enemy, Combat::Tuning::BreakDuration);
		}
	}

	void KillEnemy(entt::registry& Reg, entt::entity Enemy, const Math::Vector3& Center)
	{
		Reg.emplace_or_replace<DeadTag>(Enemy);
		Graphics::Color tint = { 1.0f, 0.8f, 0.5f, 1.0f };
		if (auto* combat = Reg.try_get<EnemyCombatComponent>(Enemy))
		{
			tint = combat->WeaponColor;
			combat->AttackActive = false;
		}
		Vfx::DeathBurst(Center, tint);
		GameFeel::HitStop(0.2f);
		GameFeel::SlowMotion(0.2f, 1.0f);
		GameFeel::Shake(0.9f, 0.5f);
		CombatEvents::Push(CombatEventType::EnemyKilled, Center);
	}

	/// <summary>
	/// 敵の攻撃がプレイヤーに当たった
	/// </summary>
	void EnemyHitsPlayer(entt::registry& Reg, entt::entity Weapon, entt::entity Player, const AttackPowerComponent& Attack, StatusComponet& Status)
	{
		entt::entity owner = entt::null;
		if (const auto* socket = Reg.try_get<SocketComponent>(Weapon))
		{
			owner = socket->Parent;
		}
		auto* combat = (owner != entt::null && Reg.valid(owner)) ? Reg.try_get<EnemyCombatComponent>(owner) : nullptr;
		const bool parryable = combat == nullptr || combat->Parryable;
		const float damageScale = combat != nullptr ? combat->AttackDamageScale : 1.0f;

		auto* defense = Reg.try_get<DefenseComponent>(Player);
		auto* stamina = Reg.try_get<StaminaComponent>(Player);
		const Combat::DefenseInput input = defense != nullptr ? defense->ToInput() : Combat::DefenseInput{};
		const float staminaValue = stamina != nullptr ? stamina->Value.Value : 0.0f;
		const Combat::DefenseResult result = Combat::ResolveDefense(input, parryable, staminaValue);

		const auto& playerTrans = Reg.get<Transform3D>(Player);
		const Math::Vector3 chest = playerTrans.Position + Math::Vector3(0.0f, 4.5f, 0.0f);
		Math::Vector3 contact = chest;
		Math::Vector3 dir = Math::Vector3(0, 1, 0);
		if (const auto* weaponTrans = Reg.try_get<Transform3D>(Weapon))
		{
			const Math::Vector3 blade = CombatSystem::WeaponPoint(*weaponTrans, -4.0f);
			contact = (chest + blade) * 0.5f;
			dir = chest - blade;
		}

		switch (result)
		{
		case Combat::DefenseResult::Parried:
			Vfx::ParryFlash(contact);
			GameFeel::HitStop(0.12f);
			GameFeel::SlowMotion(0.3f, 0.35f);
			GameFeel::Shake(0.6f, 0.3f);
			PlaySE("Assets/Sound/Sword.aud", 1.0f);
			PlaySE("Assets/Sound/HitEffect.aud", 0.4f);
			CombatEvents::Push(CombatEventType::Parry, contact);
			if (defense != nullptr)
			{
				defense->LockTimer = 0.0f;	//	連続パリィは許可
			}
			if (owner != entt::null)
			{
				CombatSystem::BeginStagger(Reg, owner, Combat::Tuning::ParryEnemyStagger);
				ApplyPosture(Reg, owner, Combat::Tuning::ParryPostureDamage, Reg.get<Transform3D>(owner).Position + Math::Vector3(0, 5, 0));
			}
			return;

		case Combat::DefenseResult::Guarded:
			Vfx::GuardSpark(contact);
			GameFeel::HitStop(0.05f);
			GameFeel::Shake(0.2f, 0.15f);
			PlaySE("Assets/Sound/Sword1.aud", 0.5f);
			CombatEvents::Push(CombatEventType::Guard, contact);
			if (stamina != nullptr)
			{
				Combat::ForceConsumeStamina(stamina->Value, Combat::Tuning::GuardStaminaCost);
			}
			break;

		case Combat::DefenseResult::GuardBroken:
			Vfx::HitSpark(contact, dir, { 1.0f, 0.5f, 0.15f, 1.0f }, 1.2f);
			GameFeel::HitStop(0.07f);
			GameFeel::Shake(0.5f, 0.25f);
			PlaySE("Assets/Sound/HitEffect.aud", 0.5f);
			CombatEvents::Push(CombatEventType::GuardBreak, contact);
			if (stamina != nullptr)
			{
				Combat::ForceConsumeStamina(stamina->Value, Combat::Tuning::GuardStaminaCost);
			}
			if (defense != nullptr)
			{
				defense->StunTimer = Combat::Tuning::GuardBreakStun;
				defense->DamageFlashTimer = 0.4f;
			}
			break;

		case Combat::DefenseResult::Hit:
			Vfx::HitSpark(contact, dir, PlayerHurtColor, 1.1f);
			GameFeel::HitStop(0.07f);
			GameFeel::Shake(0.45f, 0.25f);
			PlaySE("Assets/Sound/HitEffect.aud", 0.4f);
			if (defense != nullptr)
			{
				defense->DamageFlashTimer = 0.5f;
			}
			break;
		}

		const float damage = Combat::DamageAfterDefense(result, Attack.DamageValue * damageScale);
		Status.ApplyDamage(damage);
		CombatEvents::Push(CombatEventType::DamageToPlayer, chest, damage);
		if (Status.IsDead())
		{
			Reg.emplace_or_replace<DeadTag>(Player);
		}
	}

	/// <summary>
	/// プレイヤーの攻撃(剣・スキル)が敵に当たった
	/// </summary>
	void PlayerHitsEnemy(entt::registry& Reg, entt::entity Attacker, entt::entity Enemy, const AttackPowerComponent& Attack, StatusComponet& Status)
	{
		auto* posture = Reg.try_get<PostureComponent>(Enemy);
		auto* combat = Reg.try_get<EnemyCombatComponent>(Enemy);
		const bool isSkill = Reg.all_of<SocketComponent>(Attacker) == false;

		bool finisher = isSkill;
		const entt::entity player = FindPlayer(Reg);
		if (player != entt::null)
		{
			if (const auto* state = Reg.try_get<PlayerStateComponent>(player))
			{
				finisher |= (state->State == ePlayerState::Attack && state->Attack.AttackCount == state->Attack.AttackCountMax - 1);
			}
		}

		const float multiplier = posture != nullptr ? Combat::DamageMultiplier(posture->Value) : 1.0f;
		float damage = Attack.DamageValue * multiplier;
		if (combat != nullptr && combat->IsTrainingDummy && Status.Hp - damage < 1.0f && Reg.all_of<DummyInvulnerableTag>(Enemy))
		{
			damage = (std::max)(0.0f, Status.Hp - 1.0f);
		}
		Status.ApplyDamage(damage);
		if (combat != nullptr)
		{
			combat->HitFlashTimer = 0.12f;
		}

		const auto& enemyTrans = Reg.get<Transform3D>(Enemy);
		const Math::Vector3 contact = enemyTrans.Position + Math::Vector3(0.0f, 4.5f, 0.0f);
		Math::Vector3 dir = Math::Vector3(0, 0.5f, 0);
		if (player != entt::null)
		{
			dir = enemyTrans.Position - Reg.get<Transform3D>(player).Position + Math::Vector3(0, 1.5f, 0);
		}

		Vfx::HitSpark(contact, dir, multiplier > 1.0f ? Graphics::Color{ 1.0f, 0.8f, 0.3f, 1.0f } : PlayerSparkColor, finisher ? 1.5f : 1.0f);
		GameFeel::HitStop(finisher ? 0.09f : 0.045f);
		GameFeel::Shake(finisher ? 0.35f : 0.15f, 0.15f);
		PlaySE("Assets/Sound/HitEffect.aud", 0.25f);
		CombatEvents::Push(CombatEventType::DamageToEnemy, contact + Math::Vector3(0, 2.0f, 0), damage, multiplier > 1.0f ? 1 : 0);
		CombatEvents::Push(CombatEventType::PlayerAttackHit, contact);

		if (Status.IsDead())
		{
			KillEnemy(Reg, Enemy, contact);
			return;
		}
		ApplyPosture(Reg, Enemy, Combat::Tuning::PostureOnHit * (isSkill ? 2.0f : 1.0f), contact);
	}
}

/// <summary>
/// ダメージ処理
/// </summary>
void Engine::System::DamageSystem::PostUpdate(entt::registry& Reg, double DeltaTime)
{
#ifndef NO_DAMAGE
	auto view = Reg.view<AttackPowerComponent, CollisionEvent>();
	view.each([&](entt::entity entity, AttackPowerComponent& attack, CollisionEvent& collision)
		{
			HitHistoryComponent* history = Reg.try_get<HitHistoryComponent>(entity);
			const bool fromEnemy = Reg.all_of<EnemyWeaponTag>(entity);

			for (auto victim : collision.HitEntitys)
			{
				if (Reg.valid(victim) == false || Reg.all_of<DeadTag>(victim))
				{
					continue;
				}
				//	既に当たっている(多段防止)
				if (history != nullptr && history->HitList.contains(victim))
				{
					continue;
				}
				//	回避の無敵
				if (Reg.all_of<InvincibleComponet>(victim))
				{
					continue;
				}
				auto* status = Reg.try_get<StatusComponet>(victim);
				if (status == nullptr)
				{
					continue;
				}
				if (history != nullptr)
				{
					history->HitList.insert(victim);
				}

				if (fromEnemy)
				{
					EnemyHitsPlayer(Reg, entity, victim, attack, *status);
				}
				else
				{
					PlayerHitsEnemy(Reg, entity, victim, attack, *status);
				}
			}
		});
#endif // !NO_DAMAGE
}
