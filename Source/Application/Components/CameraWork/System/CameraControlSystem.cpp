#include "pch.h"
#include "CameraControlSystem.hpp"

#include"Application/Components/CameraWork/CameraControl.hpp"
#include"System/Input/Manager/InputManager.hpp"
#include"System/Conponent/Transform/TransformConponent.hpp"

#include"System/Camera/Camera.hpp"
#include"Math/Math.h"
#include"Application/Combat/GameFeel.hpp"
#include"Application/Components/Tag/TagComponent.hpp"
#include"Application/Components/Status/StatusComponent.hpp"
#include"Engine/LaunchOptions.hpp"

namespace
{
	constexpr float LockOnRange = 80.0f;
}


void Engine::System::CameraControlSystem::PostUpdate(entt::registry& Reg, double DeltaTime)
{
	//	コンポーネントを持つオブジェクトの取得
//	入力の取得
	auto view = Reg.view<CameraControlComponent>();
	auto input = Input::InputManager::GetInstance();

	view.each([&](auto entity, auto& ctrl)
		{
			//	ターゲットその存在確認
			if (ctrl.TargetEntity == entt::null || Reg.valid(ctrl.TargetEntity) == false)
			{
				return;
			}

			//	ロックオンの切り替え(一番近い敵)
			auto& TargetTransformForLock = Reg.get<Transform3D>(ctrl.TargetEntity);
			//	自動テストでは常にロックオンして経路を通す
			const bool autoLock = Core::LaunchOptions::Get().AutoPlay && ctrl.LockTarget == entt::null;
			if (input->IsActionPressed("LockOn") || autoLock)
			{
				if (ctrl.LockTarget != entt::null)
				{
					ctrl.LockTarget = entt::null;
				}
				else
				{
					float best = LockOnRange * LockOnRange;
					auto enemies = Reg.view<EnemyTag, Transform3D, StatusComponet>(entt::exclude<DeadTag>);
					enemies.each([&](entt::entity e, Transform3D& t, StatusComponet&)
						{
							const float d = (t.Position - TargetTransformForLock.Position).SqrLength();
							if (d < best)
							{
								best = d;
								ctrl.LockTarget = e;
							}
						});
				}
			}
			if (ctrl.LockTarget != entt::null && (Reg.valid(ctrl.LockTarget) == false || Reg.all_of<DeadTag>(ctrl.LockTarget)))
			{
				ctrl.LockTarget = entt::null;
			}

			//	入力の取得
			Math::Vector2 look = input->GetLookAxis();

			//	角度の更新
			ctrl.Yaw += look.x * ctrl.LookSensitivity * DeltaTime;
			ctrl.Pitch += look.y * ctrl.LookSensitivity * DeltaTime;

			ctrl.Yaw = fmodf(ctrl.Yaw, Math::PI * 2.0f);

			//	回転の制限
			ctrl.Pitch = std::clamp(ctrl.Pitch, ctrl.PitchLimitMin, ctrl.PitchLimitMax);

			//	注視点の計算
			auto& TargetTransform = Reg.get<Transform3D>(ctrl.TargetEntity);
			Math::Vector3 LookAtPos = TargetTransform.Position + ctrl.offset;

			//	ロックオン中は敵が画面に入るように回り込む
			if (ctrl.LockTarget != entt::null)
			{
				const Math::Vector3 enemyPos = Reg.get<Transform3D>(ctrl.LockTarget).Position;
				const Math::Vector3 to = enemyPos - TargetTransform.Position;
				const float targetYaw = atan2f(to.x, to.z);
				float diff = fmodf(targetYaw - ctrl.Yaw + Math::PI * 3.0f, Math::PI * 2.0f) - Math::PI;
				const float follow = (std::min)(1.0f, static_cast<float>(DeltaTime) * 8.0f);
				ctrl.Yaw += diff * follow;
				ctrl.Pitch += (-0.22f - ctrl.Pitch) * (std::min)(1.0f, static_cast<float>(DeltaTime) * 4.0f);
				LookAtPos = LookAtPos + (enemyPos + Math::Vector3(0.0f, 5.0f, 0.0f) - LookAtPos) * 0.3f;
			}

			//	球面座標からカメラの相対座標を算出
			// 公式: （間違っていたらどんまい）
			// x = r * cos(pitch) * sin(yaw)
			// y = r * sin(pitch)
			// z = r * cos(pitch) * cos(yaw)
			Math::Vector3 relativePos;
			float cosPitch = cosf(ctrl.Pitch);

			relativePos.x = -ctrl.Distance * cosPitch * sinf(ctrl.Yaw);
			relativePos.y = -ctrl.Distance * sinf(ctrl.Pitch);
			relativePos.z = -ctrl.Distance * cosPitch * cosf(ctrl.Yaw);

			//	カメラの最終的な座標
			Math::Vector3 cameraPos = LookAtPos + relativePos;

			//	ロックオン中は肩越し視点(自分で敵を隠さない)
			if (ctrl.LockTarget != entt::null)
			{
				Math::Vector3 forward = LookAtPos - cameraPos;
				forward.y = 0.0f;
				forward.Normalize();
				const Math::Vector3 right = { forward.z, 0.0f, -forward.x };
				cameraPos += right * 3.5f - forward * 3.0f;
				LookAtPos += right * 1.5f;
			}

			//	手応えの揺れ
			const Math::Vector3 shake = Combat::GameFeel::GetShakeOffset();
			cameraPos += shake;
			LookAtPos += shake * 0.6f;

			//	カメラ本体に反映
			if (Camera::Main != nullptr)
			{
				Camera::Main->Update(cameraPos, LookAtPos, Math::Vector3::Up);
			}

		});
}
