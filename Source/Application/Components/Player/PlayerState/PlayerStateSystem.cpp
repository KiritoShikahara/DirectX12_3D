#include "pch.h"
#include "PlayerStateSystem.hpp"
#include "Application/Components/Tag/TagComponent.hpp"
#include"Application/Components/InputMove/MoveComponent.hpp"
#include"Application/Components/WeaponAttack/WeaponAttackComponent.hpp"
#include"Application/Components/Invincible/InvincibleTag.hpp"

#include"System/Conponent/Collider/ColliderComponent.hpp"
#include"System/Conponent/Transform/TransformConponent.hpp"


#include"Application/Components/Skill/SkillComponent.hpp"


#include"Audio/Manager/AudioManager.hpp"
#include"Application/Combat/CombatComponents.hpp"
#include"Application/Components/CameraWork/CameraControl.hpp"
#include"Application/Combat/CombatEvents.hpp"
#include"Application/Combat/GameFeel.hpp"
#include"Application/Effects/VfxPresets.hpp"

/// <summary>
/// 走り状態に移行できるかどうかの判定
/// </summary>
/// <returns></returns>
bool Engine::System::PlayerStateSystem::CheckRunRequest(PlayerStateComponent& state)
{
	//	歩き状態かスプリント状態かダッシュ状態ならtrue
	//	any_ofより処理速度が速いので可読性そんなに変わらないからこれで。
	const auto& curr = state.State;
	return  
		curr == ePlayerState::Idle	||
		curr == ePlayerState::Run;

}

/// <summary>
/// 攻撃状態に移行できるかどうかの判定
/// </summary>
/// <returns></returns>
bool Engine::System::PlayerStateSystem::CheckAttackRequest(PlayerStateComponent& state)
{
	const auto& curr = state.State;
	return
		curr == ePlayerState::Idle ||
		curr == ePlayerState::Run ||
		curr == ePlayerState::Dodge ||
		curr == ePlayerState::Attack;
}

/// <summary>
/// スプリント状態に移行できるかどうかの判定
/// </summary>
/// <param name="state"></param>
/// <returns></returns>
bool Engine::System::PlayerStateSystem::CheckSprintRequest(PlayerStateComponent& state, const Math::Vector3& InputVec)
{
	const auto& curr = state.State;

	//	入力量がないとだめ
	if (InputVec.SqrLength() <= 0)
	{
		return false;
	}

	//	回数制限はスタミナで管理する
	return 
		curr == ePlayerState::Idle ||
		curr == ePlayerState::Run ||
		curr == ePlayerState::Attack ||
		curr == ePlayerState::Guard;
}

/// <summary>
/// 状態の終了
/// </summary>
/// <param name="Reg"></param>
void Engine::System::PlayerStateSystem::ExitState(entt::registry& Reg)
{
	auto view = Reg.view<PlayerStateComponent, InputRequestComponent, FbxComponent>();
	view.each([&](auto entity, PlayerStateComponent& state, InputRequestComponent& req, FbxComponent& fbx)
		{
			//	攻撃
			switch (state.State)
			{
			case ePlayerState::Attack:
				OnAttackFinished(Reg, entity, state, req, fbx);
				break;
			case ePlayerState::Run:
				state.State = ePlayerState::Idle;
				fbx.CurrAnimation = "Idle";
				fbx.IsLoop = true;
				break;
			case ePlayerState::Dodge:
				state.State = ePlayerState::Idle;
				fbx.CurrAnimation = "Idle";
				fbx.AnimationScale = 1.0f;
				fbx.IsLoop = true;
				break;
			default:
				break;
			}


		});

}

/// <summary>
/// 状態の切り替え
/// </summary>
void Engine::System::PlayerStateSystem::ChangeState(entt::registry& Reg, PlayerStateComponent& state, ePlayerState NextState)
{
	ExitState(Reg);
	state.State = NextState;
}


/// <summary>
/// 攻撃の終了条件の判定
/// </summary>
/// <returns>true:終了</returns>
bool Engine::System::PlayerStateSystem::IsFinishAttack(PlayerStateComponent& State, InputRequestComponent& Req, FbxComponent& Fbx,float DeltaTime)
{
	//	アニメーション終了判定
	if (Fbx.Mesh->GetAnimationFinish() == false)
	{
		return false;
	}

	//	時間経過
	State.Attack.AttackInputTimer += DeltaTime;

	//	受付時間を終了していたら終了
	if (State.Attack.AttackInputTimer <= State.Attack.AttackInputWindow)
	{
		return false;
	}

	return true;
}

/// <summary>
/// 攻撃の終了
/// </summary>
void Engine::System::PlayerStateSystem::OnAttackFinished(entt::registry& Reg,entt::entity entity, PlayerStateComponent& state, InputRequestComponent& req, FbxComponent& fbx)
{
	state.State = ePlayerState::Idle;
	state.Attack.AttackQueued = false;
	fbx.CurrAnimation = "Idle";
	fbx.IsLoop = true;
	fbx.AnimationScale = 1.0f;

	//	当たり判定の削除
	if (Reg.all_of<ColliderComponent>(state.Weapon))
	{
		Reg.remove<ColliderComponent>(state.Weapon);
	}
	if (Reg.all_of<HitHistoryComponent>(state.Weapon))
	{
		Reg.remove<HitHistoryComponent>(state.Weapon);
	}
}

/// <summary>
/// 終了条件を判断するして前状態を終了する
/// </summary>
void Engine::System::PlayerStateSystem::PreUpdate(entt::registry& Reg, double DeltaTime)
{
	//	攻撃の終了などを判断する
	auto view = Reg.view<PlayerStateComponent, InputRequestComponent, FbxComponent>();
	view.each([&](auto entity, PlayerStateComponent& state, InputRequestComponent& req, FbxComponent& fbx)
		{
			//	攻撃
			if (state.State == ePlayerState::Attack)
			{
				//	攻撃の終了判定
				if (IsFinishAttack(state, req, fbx, DeltaTime))
				{
					ExitState(Reg);
				}
			}
			//	ダッシュ
			else if (state.State == ePlayerState::Run)
			{
				if (req.InputVec.SqrLength() <= 0)
				{
					ExitState(Reg);
				}
			}
			//	ここだけど状態の自動終了と無敵は分離して考えて、アニメーションの終了とかにするほうがいいかもしれない
			else if (state.State == ePlayerState::Dodge)
			{
				if (fbx.Mesh->GetAnimationFinish() == true)
				{
					ExitState(Reg);
				}
			}

			//	タイマーのカウントダウン
			if (state.Dodge.RecoveryTimer > 0.0f)
			{
				state.Dodge.RecoveryTimer -= DeltaTime;
			}
			//	クールタイム終了判定
			if (state.Dodge.IsCoolDown && state.Dodge.RecoveryTimer <= 0.0f)
			{
				state.Dodge.IsCoolDown = false;
				state.Dodge.DodgeCount = 0;
			}
			//	1回以上で最大数以下の状態で連続判定の時間を過ぎたらカウントをリセットする
			else if (!state.Dodge.IsCoolDown && state.Dodge.RecoveryTimer <= 0.0f)
			{
				state.Dodge.DodgeCount = 0;
			}
		});

}


/// <summary>
/// スキルの発動
/// </summary>
void Engine::System::PlayerStateSystem::TryActivateSkill(entt::registry& Reg, entt::entity Player, int Slot, const Math::Vector3& Position)
{
	SkillSlotComponent* slots = Reg.try_get<SkillSlotComponent>(Player);
	if (slots == nullptr || Slot < 0 || Slot >= static_cast<int>(slots->SkillSlots.size()))
	{
		return;
	}
	const entt::entity skillEntity = slots->SkillSlots[Slot];
	if (Reg.valid(skillEntity) == false || Reg.all_of<SkillComponent>(skillEntity) == false)
	{
		return;
	}
	SkillComponent& skill = Reg.get<SkillComponent>(skillEntity);
	if (skill.CooldownTimer > 0.0f)
	{
		return;
	}

	Transform3D& skillTrans = Reg.get<Transform3D>(skillEntity);
	skillTrans.Position = Position;

	//	有効化と当たり判定
	Reg.emplace_or_replace<ActiveSkillTag>(skillEntity);
	Reg.emplace_or_replace<HitHistoryComponent>(skillEntity);
	auto col = ColliderComponent::Create<OBBCollider>();
	col.GetPtr<OBBCollider>()->SetVolume({ skill.Radius * 2.0f, 0.4f, skill.Radius * 2.0f });
	Reg.emplace_or_replace<ColliderComponent>(skillEntity, std::move(col));

	//	演出
	if (skill.Kind == SkillComponent::eKind::Wind)
	{
		Vfx::SkillWind(Position, skill.Radius);
		Audio::AudioManager::GetInstance()->PlaySE("Assets/Sound/Wind.aud", false, 1.0f);
	}
	else
	{
		Vfx::SkillFire(Position, skill.Radius);
		Audio::AudioManager::GetInstance()->PlaySE("Assets/Sound/Fire.aud", false, 1.0f);
	}
	Combat::GameFeel::Shake(0.3f, 0.3f);
	Combat::CombatEvents::Push(Combat::CombatEventType::SkillUsed, Position, 0.0f, Slot);

	skill.CooldownTimer = skill.MaxCooldown;
}

/// <summary>
/// 状態遷移の判定
/// </summary>
void Engine::System::PlayerStateSystem::MainUpdate(entt::registry& Reg, double DeltaTime)
{
	//	優先度順: スタン > 回避 > ガード > スキル > 攻撃 > 走り > 待機
	auto view = Reg.view<Transform3D,PlayerStateComponent, InputRequestComponent,FbxComponent, MoveComponent>(entt::exclude<DeadTag>);
	bool ret = false;

	view.each([&](auto entity, Transform3D& trans,PlayerStateComponent& state, InputRequestComponent& req, FbxComponent&fbx, MoveComponent& move)
		{
			DefenseComponent* defense = Reg.try_get<DefenseComponent>(entity);
			StaminaComponent* stamina = Reg.try_get<StaminaComponent>(entity);

			//	ガード崩しで動けない
			if (defense != nullptr && defense->StunTimer > 0.0f)
			{
				if (state.State != ePlayerState::Idle)
				{
					ChangeState(Reg, state, ePlayerState::Idle);
				}
				fbx.CurrAnimation = "Idle";
				fbx.AnimationScale = 0.2f;
				move.InputDir = Math::Vector3::Zero;
				return;
			}

			//	回避
			ret = (HasFlag(req.Flags, eActionInputFlags::DodgeRequested) == true)
				&& (this->CheckSprintRequest(state, req.InputVec) == true);
			if (ret == true && stamina != nullptr && Combat::TryConsumeStamina(stamina->Value, Combat::Tuning::DodgeStaminaCost) == false)
			{
				//	スタミナ不足を知らせる
				stamina->DeniedFlashTimer = 0.5f;
				Combat::CombatEvents::Push(Combat::CombatEventType::DodgeDenied, trans.Position);
				ret = false;
			}
			if (ret == true)
			{
				ChangeState(Reg, state, ePlayerState::Dodge);
				fbx.CurrAnimation = "Dodge";
				fbx.Mesh->RestartAnimation();
				fbx.AnimationScale = 4.0f;
				fbx.IsLoop = false;
				Reg.emplace_or_replace<InvincibleComponet>(entity);
				state.Dodge.DodgeCount++;
				state.Dodge.RecoveryTimer = state.Dodge.DodgeInputWindow;

				//	入力方向へ回避(入力が無ければ前)
				move.ForceVelocity = trans.GetForward();
				Vfx::DodgeDust(trans.Position);
				Audio::AudioManager::GetInstance()->PlaySE("Assets/Sound/Foot.aud", false, 0.4f);
				Combat::CombatEvents::Push(Combat::CombatEventType::Dodge, trans.Position);
				return;
			}
			if (state.State == ePlayerState::Dodge)
			{
				return;
			}

			//	ガード(押した瞬間はパリィ受付)
			const bool guardHeld = defense != nullptr && defense->Holding;
			if (guardHeld)
			{
				if (state.State != ePlayerState::Guard)
				{
					ChangeState(Reg, state, ePlayerState::Guard);
					fbx.CurrAnimation = "Idle";
					fbx.AnimationScale = 0.4f;
					fbx.IsLoop = true;
				}
				move.InputDir = Math::Vector3::Zero;
				return;
			}
			if (state.State == ePlayerState::Guard)
			{
				state.State = ePlayerState::Idle;
				fbx.AnimationScale = 1.0f;
			}

			//	スキル
			if (HasFlag(req.Flags, eActionInputFlags::Skill1Requested))
			{
				TryActivateSkill(Reg, entity, 0, trans.Position);
				return;
			}
			if (HasFlag(req.Flags, eActionInputFlags::Skill2Requested))
			{
				TryActivateSkill(Reg, entity, 1, trans.Position);
				return;
			}

			// 攻撃ボタンが押されている
			bool isRequested = HasFlag(req.Flags, eActionInputFlags::AttackRequested);

			// 先行入力
			bool canCombo = (state.State == ePlayerState::Attack && state.Attack.AttackQueued);

			//	攻撃
			ret = (isRequested || canCombo);
			if (ret == true)
			{
				// すでに攻撃中の場合はコンボ加算、そうでなければ初期化
				if (state.State == ePlayerState::Attack) 
				{
					//	攻撃中なら次の攻撃にまだ遷移しない
					if (fbx.Mesh->GetAnimationFinish() == false)
					{
						state.Attack.AttackInputTimer = 0.0f;
						state.Attack.AttackQueued = true;
						return;
					}

					//	リセット
					state.Attack.AttackCount++;
					state.Attack.AttackQueued = false;
					//	攻撃数が最大を越したらリセット
					if (state.Attack.AttackCount >= state.Attack.AttackCountMax)
					{
						state.Attack.AttackCount = 0;
					}
				}
				else 
				{
					ChangeState(Reg, state, ePlayerState::Attack);
					state.Attack.AttackCount = 0;
					fbx.IsLoop = false;
					fbx.AnimationScale = 2.2f;
				}

				//	武器に必要なものをアタッチ
				Reg.emplace_or_replace<HitHistoryComponent>(state.Weapon);
				//	ここで当たり判定をアタッチする。
				auto col = ColliderComponent::Create<OBBCollider>();
				auto collider = col.GetPtr<OBBCollider>();
				collider->SetVolume({ 2.0f,5.0f,2.0f });
				col.Offset = { 0.0f, -4.0f, 0.0f };
				Reg.emplace_or_replace<ColliderComponent>(state.Weapon, std::move(col));

				//	ロックオン中は敵の方へ振り向いて斬る
				if (const auto* camera = Reg.try_get<CameraControlComponent>(entity);
					camera != nullptr && camera->LockTarget != entt::null && Reg.valid(camera->LockTarget))
				{
					const Math::Vector3 to = Reg.get<Transform3D>(camera->LockTarget).Position - trans.Position;
					trans.Rotation = Math::Quaternion::AngleAxis(atan2f(to.x, to.z), Math::Vector3(0, 0, 1));
				}

				fbx.CurrAnimation = "Attack_" + std::to_string(state.Attack.AttackCount);
				fbx.Mesh->RestartAnimation();
				Audio::AudioManager::GetInstance()->PlaySE("Assets/Sound/Sword1.aud", false, 0.1f);

				return;

			}

			//	走り
			ret = (HasFlag(req.Flags,eActionInputFlags::RunRequested) == true) && (this->CheckRunRequest(state) == true);
			if (ret == true)
			{
				static float FootStepTimer = 0.0f;
				constexpr float FootStepInterval = 0.3f;

				if (state.State != ePlayerState::Run)
				{
					state.State = ePlayerState::Run;
					fbx.CurrAnimation = "Jog";
					fbx.AnimationScale = 1.0f;
				}

				FootStepTimer += DeltaTime;
				if (FootStepTimer >= FootStepInterval)
				{
					FootStepTimer = 0.0f;
					Audio::AudioManager::GetInstance()->PlaySE("Assets/Sound/Foot.aud", false, 0.1f);
				}

				//	移動量の代入
				move.InputDir = req.InputVec;
				return;
			}

			//	待機
			if (state.State == ePlayerState::Idle)
			{
				fbx.CurrAnimation = "Idle";
				fbx.AnimationScale = 1.0f;
				fbx.IsLoop = true;
				move.InputDir = Math::Vector3::Zero;
			}
		});
}

/// <summary>
/// 今の状態からアニメーションの設定と更新
/// </summary>
void Engine::System::PlayerStateSystem::PostUpdate(entt::registry& Reg, double DeltaTime)
{

}
