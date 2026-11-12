#include "pch.h"
#include "CombatSystem.hpp"

#include"Application/Combat/CombatComponents.hpp"
#include"Application/Combat/CombatEvents.hpp"
#include"Application/Effects/VfxPresets.hpp"
#include"Application/Components/Tag/TagComponent.hpp"
#include"Application/Components/Player/Input/InputRequestComponent.hpp"
#include"Application/Components/Player/PlayerState/PlayerStateComponent.hpp"
#include"Application/Components/Enemy/AI/EnemyAIComponent.hpp"
#include"Application/Components/Enemy/Parameter/EnemyParametersComponent.hpp"
#include"Application/Components/WeaponAttack/WeaponAttackComponent.hpp"

#include"System/Conponent/Collider/ColliderComponent.hpp"
#include"System/Conponent/Transform/TransformConponent.hpp"
#include"System/Conponent/Fbx/FbxMeshConponent.hpp"
#include"System/Conponent/Rigidbody/RigidbodyComponent.hpp"
#include"Graphics/Particles/ParticleSystem.hpp"
#include"Audio/Manager/AudioManager.hpp"
#include"Math/Random/Random.hpp"

#include<algorithm>

namespace
{
	using namespace Engine;
	using namespace Engine::System;

	void RemoveWeaponCollider(entt::registry& Reg, entt::entity Weapon)
	{
		if (Weapon == entt::null || Reg.valid(Weapon) == false)
		{
			return;
		}
		if (Reg.all_of<ColliderComponent>(Weapon))
		{
			Reg.remove<ColliderComponent>(Weapon);
		}
	}

	void AttachWeaponCollider(entt::registry& Reg, entt::entity Weapon)
	{
		if (Weapon == entt::null || Reg.valid(Weapon) == false || Reg.all_of<ColliderComponent>(Weapon))
		{
			return;
		}
		auto col = ColliderComponent::Create<OBBCollider>();
		col.GetPtr<OBBCollider>()->SetVolume({ 1.2f, 5.0f, 1.2f });
		col.Offset = { 0.0f, -4.0f, 0.0f };
		Reg.emplace<ColliderComponent>(Weapon, std::move(col));
	}

	float SmoothStep(float e0, float e1, float x)
	{
		const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}
}

namespace Engine::System
{
	Math::Vector3 CombatSystem::WeaponPoint(const Transform3D& WeaponTransform, float LocalY)
	{
		const Math::Matrix rot = WeaponTransform.Rotation.ToMatrix();
		return WeaponTransform.Position + Math::Vector3::TransformNormal(Math::Vector3(0.0f, LocalY, 0.0f), rot);
	}

	void CombatSystem::BeginStagger(entt::registry& Reg, entt::entity Enemy, float Seconds)
	{
		auto* combat = Reg.try_get<EnemyCombatComponent>(Enemy);
		if (combat == nullptr)
		{
			return;
		}
		combat->StaggerTimer = (std::max)(combat->StaggerTimer, Seconds);
		combat->AttackActive = false;
		Reg.emplace_or_replace<StaggeredTag>(Enemy);

		if (auto* param = Reg.try_get<EnemyParameters>(Enemy))
		{
			RemoveWeaponCollider(Reg, param->Weapon);
		}
		if (auto* ai = Reg.try_get<EnemyAIComponent>(Enemy))
		{
			ai->CurrState = eEnemyState::Idle;
			ai->StateTimer = 0.0f;
			ai->CurrAttackCombo = 0;
			ai->IsActionFinished = false;
		}
		if (auto* fbx = Reg.try_get<FbxComponent>(Enemy))
		{
			fbx->CurrAnimation = "Idle";
			fbx->AnimationScale = 0.25f;
			fbx->IsLoop = true;
		}
		if (auto* rigid = Reg.try_get<Rigidbody3D>(Enemy))
		{
			rigid->Velocity = Math::Vector3::Zero;
		}
	}

	void CombatSystem::PreUpdate(entt::registry& Reg, double DeltaTime)
	{
		const float dt = static_cast<float>(DeltaTime);

		//	防御入力とスタミナ(プレイヤー)
		auto players = Reg.view<DefenseComponent, InputRequestComponent>();
		players.each([&](DefenseComponent& defense, InputRequestComponent& req)
			{
				defense.TimeSincePress += dt;
				defense.LockTimer = (std::max)(0.0f, defense.LockTimer - dt);
				defense.StunTimer = (std::max)(0.0f, defense.StunTimer - dt);
				defense.DamageFlashTimer = (std::max)(0.0f, defense.DamageFlashTimer - dt);

				const bool stunned = defense.StunTimer > 0.0f;
				defense.Holding = HasFlag(req.Flags, eActionInputFlags::GuardHeld) && stunned == false;
				if (HasFlag(req.Flags, eActionInputFlags::GuardPressed) && stunned == false && defense.LockTimer <= 0.0f)
				{
					defense.TimeSincePress = 0.0f;
					defense.LockTimer = Combat::Tuning::ParryWindow + Combat::Tuning::ParryLockout;
				}
			});

		auto staminas = Reg.view<StaminaComponent>();
		staminas.each([&](StaminaComponent& stamina)
			{
				Combat::UpdateStamina(stamina.Value, dt);
				stamina.DeniedFlashTimer = (std::max)(0.0f, stamina.DeniedFlashTimer - dt);
			});

		auto postures = Reg.view<PostureComponent>(entt::exclude<DeadTag>);
		postures.each([&](PostureComponent& posture)
			{
				Combat::UpdatePosture(posture.Value, dt);
			});
	}

	void CombatSystem::MainUpdate(entt::registry& Reg, double DeltaTime)
	{
		const float dt = static_cast<float>(DeltaTime);
		UpdateStagger(Reg, dt);
		UpdateEnemyAttacks(Reg, dt);
	}

	void CombatSystem::UpdateStagger(entt::registry& Reg, float DeltaTime)
	{
		auto view = Reg.view<EnemyCombatComponent, Transform3D>(entt::exclude<DeadTag>);
		view.each([&](entt::entity entity, EnemyCombatComponent& combat, Transform3D& trans)
			{
				combat.HitFlashTimer = (std::max)(0.0f, combat.HitFlashTimer - DeltaTime);

				const auto* posture = Reg.try_get<PostureComponent>(entity);
				const bool broken = posture != nullptr && posture->Value.IsBroken();
				if (broken)
				{
					Vfx::StunSparkle(trans.Position + Math::Vector3(0.0f, 9.0f, 0.0f));
				}

				if (Reg.all_of<StaggeredTag>(entity) == false)
				{
					return;
				}
				if (auto* rigid = Reg.try_get<Rigidbody3D>(entity))
				{
					rigid->Velocity = Math::Vector3::Zero;
				}
				combat.StaggerTimer -= DeltaTime;
				if (combat.StaggerTimer <= 0.0f && broken == false)
				{
					combat.StaggerTimer = 0.0f;
					Reg.remove<StaggeredTag>(entity);
					if (auto* ai = Reg.try_get<EnemyAIComponent>(entity))
					{
						ai->CurrState = eEnemyState::Idle;
						ai->StateTimer = 0.0f;
						ai->IsActionFinished = false;
					}
					if (auto* fbx = Reg.try_get<FbxComponent>(entity))
					{
						fbx->AnimationScale = 1.0f;
					}
				}
			});
	}

	void CombatSystem::UpdateEnemyAttacks(entt::registry& Reg, float DeltaTime)
	{
		auto view = Reg.view<EnemyAIComponent, EnemyParameters, EnemyCombatComponent, FbxComponent>(entt::exclude<DeadTag>);
		view.each([&](entt::entity entity, EnemyAIComponent& ai, EnemyParameters& param, EnemyCombatComponent& combat, FbxComponent& fbx)
			{
				//	被弾フラッシュ
				fbx.Mesh->SetEmissive({ 1.0f, 0.95f, 0.9f, 1.0f }, combat.HitFlashTimer > 0.0f ? (combat.HitFlashTimer / 0.12f) * 0.3f : 0.0f);

				FbxComponent* weaponFbx = (param.Weapon != entt::null && Reg.valid(param.Weapon)) ? Reg.try_get<FbxComponent>(param.Weapon) : nullptr;
				const Transform3D* weaponTrans = (param.Weapon != entt::null && Reg.valid(param.Weapon)) ? Reg.try_get<Transform3D>(param.Weapon) : nullptr;

				const bool attacking = ai.CurrState == eEnemyState::Attacking && Reg.all_of<StaggeredTag>(entity) == false;
				if (attacking == false)
				{
					combat.AttackActive = false;
					combat.LastProgress = 0.0f;
					RemoveWeaponCollider(Reg, param.Weapon);
					if (weaponFbx != nullptr)
					{
						weaponFbx->Mesh->SetEmissive(combat.WeaponColor, 0.6f);
					}
					return;
				}

				const float progress = fbx.Mesh->GetAnimationProgress();

				//	新しい攻撃の開始(進行率が戻った)
				if (progress + 0.05f < combat.LastProgress || combat.LastProgress <= 0.0f)
				{
					combat.TelegraphFired = false;
					combat.Parryable = combat.IsTrainingDummy || Random::Range(0.0f, 1.0f) >= combat.UnparryableChance;
					Reg.emplace_or_replace<HitHistoryComponent>(param.Weapon);
				}
				combat.LastProgress = (std::max)(progress, 0.001f);

				const Graphics::Color glowColor = combat.Parryable ? combat.WeaponColor : Graphics::Color{ 1.0f, 0.08f, 0.05f, 1.0f };
				const Combat::AttackPhase phase = Combat::GetAttackPhase(progress);

				//	予告閃光
				if (combat.TelegraphFired == false && progress >= Combat::Tuning::TelegraphAt)
				{
					combat.TelegraphFired = true;
					if (weaponTrans != nullptr)
					{
						const Math::Vector3 pos = WeaponPoint(*weaponTrans, -4.0f);
						Vfx::Telegraph(pos, combat.Parryable);
					}
					//	HUDの予告マークは敵の頭上に出す
					const Math::Vector3 head = Reg.get<Transform3D>(entity).Position + Math::Vector3(0.0f, 11.0f, 0.0f);
					Combat::CombatEvents::Push(Combat::CombatEventType::Telegraph, head, 0.0f, combat.Parryable ? 1 : 0);
					Audio::AudioManager::GetInstance()->PlaySE("Assets/Sound/Select.aud", false, combat.Parryable ? 0.35f : 0.6f);
				}

				//	武器の発光(予備動作で段々強く)
				if (weaponFbx != nullptr)
				{
					float intensity = 0.6f;
					switch (phase)
					{
					case Combat::AttackPhase::Windup:
						intensity = 0.6f + SmoothStep(0.0f, Combat::Tuning::ActiveBegin, progress) * 2.8f;
						break;
					case Combat::AttackPhase::Active:
						intensity = 4.0f;
						break;
					case Combat::AttackPhase::Recovery:
						intensity = 0.6f + (1.0f - SmoothStep(Combat::Tuning::ActiveEnd, 1.0f, progress)) * 4.0f;
						break;
					}
					weaponFbx->Mesh->SetEmissive(glowColor, intensity);
				}

				//	攻撃判定は攻撃フェーズの間だけ
				combat.AttackActive = (phase == Combat::AttackPhase::Active);
				if (combat.AttackActive)
				{
					AttachWeaponCollider(Reg, param.Weapon);
				}
				else
				{
					RemoveWeaponCollider(Reg, param.Weapon);
				}
			});
	}

	void CombatSystem::PostUpdate(entt::registry& Reg, double DeltaTime)
	{
		auto particles = Graphics::ParticleSystem::GetInstance();

		//	武器の軌跡
		auto trails = Reg.view<WeaponTrailComponent, Transform3D>();
		trails.each([&](WeaponTrailComponent& trail, Transform3D& trans)
			{
				bool emitting = false;
				if (trail.Owner != entt::null && Reg.valid(trail.Owner) && Reg.all_of<DeadTag>(trail.Owner) == false)
				{
					if (const auto* state = Reg.try_get<PlayerStateComponent>(trail.Owner))
					{
						emitting = state->State == ePlayerState::Attack;
					}
					else if (const auto* combat = Reg.try_get<EnemyCombatComponent>(trail.Owner))
					{
						emitting = combat->AttackActive;
					}
				}
				particles->UpdateTrail(trail.Trail, WeaponPoint(trans, trail.BaseLocal), WeaponPoint(trans, trail.TipLocal), emitting);
			});

		//	ガード中の光
		auto guards = Reg.view<DefenseComponent, Transform3D>(entt::exclude<DeadTag>);
		guards.each([&](DefenseComponent& defense, Transform3D& trans)
			{
				if (defense.Holding)
				{
					Vfx::GuardAura(trans.Position);
				}
			});
	}
}
