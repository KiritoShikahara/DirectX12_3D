#include "pch.h"
#include "InputRequestSystem.hpp"
#include"InputRequestComponent.hpp"
#include"System/Input/Manager/InputManager.hpp"
#include"System/Conponent/Transform/TransformConponent.hpp"
#include"System/Camera/Camera.hpp"
#include"Engine/LaunchOptions.hpp"

#include"Application/Combat/CombatComponents.hpp"
#include"Application/Components/Tag/TagComponent.hpp"
#include"Application/Components/Enemy/AI/EnemyAIComponent.hpp"
#include"Application/Components/Enemy/Parameter/EnemyParametersComponent.hpp"
#include"System/Conponent/Fbx/FbxMeshConponent.hpp"

#include"Math/Vector2/Vector2.h"

namespace
{
	using namespace Engine::System;

	void SetFlag(eActionInputFlags& Flags, eActionInputFlags Flag, bool On)
	{
		if (On)
		{
			Flags |= Flag;
		}
		else
		{
			Flags &= ~Flag;
		}
	}

	/// <summary>
	/// 自動テスト用の操作(--autoplay)。近づいて斬り、予告を見たら弾き、たまに回避とスキルを使う
	/// </summary>
	void AutoPilot(entt::registry& Reg, entt::entity Player, InputRequestComponent& Req, float DeltaTime)
	{
		static float sTime = 0.0f;
		static float sGuardHold = 0.0f;
		static float sNextSkill = 4.0f;
		sTime += DeltaTime;
		Req.Flags = eActionInputFlags::None;
		Req.InputVec = { 0.0f, 0.0f, 0.0f };

		const auto* playerTrans = Reg.try_get<Transform3D>(Player);
		if (playerTrans == nullptr)
		{
			return;
		}

		//	一番近い敵
		entt::entity target = entt::null;
		float bestDistance = 1e9f;
		auto enemies = Reg.view<EnemyTag, Transform3D>(entt::exclude<DeadTag>);
		enemies.each([&](entt::entity e, Transform3D& t)
			{
				const float d = (t.Position - playerTrans->Position).SqrLength();
				if (d < bestDistance)
				{
					bestDistance = d;
					target = e;
				}
			});
		if (target == entt::null)
		{
			return;
		}

		const Math::Vector3 toEnemy = Reg.get<Transform3D>(target).Position - playerTrans->Position;
		const float distance = std::sqrt(bestDistance);

		//	予告を見たら弾く(押して少し保持)
		bool threat = false;
		if (const auto* combat = Reg.try_get<EnemyCombatComponent>(target))
		{
			if (const auto* fbx = Reg.try_get<FbxComponent>(target); fbx != nullptr && combat->LastProgress > 0.0f)
			{
				const float p = fbx->Mesh->GetAnimationProgress();
				threat = combat->Parryable && p > 0.30f && p < 0.42f && distance < 14.0f;
			}
		}
		if (threat && sGuardHold <= 0.0f)
		{
			SetFlag(Req.Flags, eActionInputFlags::GuardPressed, true);
			sGuardHold = 0.3f;
		}
		if (sGuardHold > 0.0f)
		{
			sGuardHold -= DeltaTime;
			SetFlag(Req.Flags, eActionInputFlags::GuardHeld, true);
			return;
		}

		//	カメラ基準の入力に直す
		Math::Vector3 forward = { 0, 0, 1 };
		Math::Vector3 right = { 1, 0, 0 };
		if (Camera::Main != nullptr)
		{
			forward = Camera::Main->GetForward();
			right = Camera::Main->GetRight();
			forward.y = 0.0f;
			right.y = 0.0f;
			forward.Normalize();
			right.Normalize();
		}
		Math::Vector3 dir = toEnemy;
		dir.y = 0.0f;
		dir.Normalize();
		const Math::Vector3 input = { Math::Vector3::Dot(dir, right), 0.0f, Math::Vector3::Dot(dir, forward) };

		if (distance > 6.5f)
		{
			Req.InputVec = input;
			SetFlag(Req.Flags, eActionInputFlags::RunRequested, true);
		}
		else
		{
			//	向きを合わせてから攻撃
			Req.InputVec = input * 0.01f;
			SetFlag(Req.Flags, eActionInputFlags::RunRequested, true);
			SetFlag(Req.Flags, eActionInputFlags::AttackRequested, std::fmod(sTime, 0.35f) < DeltaTime);
		}

		if (sTime >= sNextSkill)
		{
			sNextSkill = sTime + 5.0f;
			SetFlag(Req.Flags, (static_cast<int>(sTime) % 2 == 0) ? eActionInputFlags::Skill1Requested : eActionInputFlags::Skill2Requested, true);
		}
		if (std::fmod(sTime, 6.0f) < DeltaTime)
		{
			Req.InputVec = input * -1.0f;
			SetFlag(Req.Flags, eActionInputFlags::DodgeRequested, true);
		}
	}
}

void Engine::System::InputRequestSystem::PreUpdate(entt::registry& Reg, double DeltaTime)
{
	auto view = Reg.view<InputRequestComponent>();

	if (Core::LaunchOptions::Get().AutoPlay)
	{
		view.each([&](entt::entity entity, InputRequestComponent& req)
			{
				AutoPilot(Reg, entity, req, static_cast<float>(DeltaTime));
			});
		return;
	}

	auto input = Input::InputManager::GetInstance();
	Math::Vector2 vel = input->GetAxis();

	view.each([&](InputRequestComponent& req)
		{
			req.InputVec = { vel.x,0.0f,vel.y };
			SetFlag(req.Flags, eActionInputFlags::RunRequested, vel.SqrLength() > 0);
			SetFlag(req.Flags, eActionInputFlags::DodgeRequested, input->IsActionPressed("Sprint"));
			SetFlag(req.Flags, eActionInputFlags::AttackRequested, input->IsActionPressed("Attack"));
			SetFlag(req.Flags, eActionInputFlags::Skill1Requested, input->IsActionPressed("Skill1"));
			SetFlag(req.Flags, eActionInputFlags::Skill2Requested, input->IsActionPressed("Skill2"));
			SetFlag(req.Flags, eActionInputFlags::GuardPressed, input->IsActionPressed("Guard"));
			SetFlag(req.Flags, eActionInputFlags::GuardHeld, input->IsActionHeld("Guard"));
		});
}
