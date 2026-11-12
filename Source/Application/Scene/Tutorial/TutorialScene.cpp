#include "pch.h"
#include "TutorialScene.hpp"

#include"System/Entity/System/Manager/SystemManager.hpp"
#include"System/Entity/Manager/EntityManager.hpp"
#include"System/Scene/Manager/SceneManager.hpp"
#include"System/Camera/Camera.hpp"
#include"System/Conponent/Collider/System/ColliderSystem.hpp"
#include"System/Input/Manager/InputManager.hpp"
#include"Graphics/SkyBox/Resource/SkyBoxResourceManager.hpp"
#include"Audio/Manager/AudioManager.hpp"
#include"Engine/LaunchOptions.hpp"

#include"Application/Components/CameraWork/System/CameraControlSystem.hpp"
#include"Application/Components/Socket/SocketComponentSystem.hpp"
#include"Application/Components/Player/Input/InputRequestSystem.hpp"
#include"Application/Components/Player/PlayerState/PlayerStateSystem.hpp"
#include"Application/Components/Damage/DamageSystem.hpp"
#include"Application/Components/InputMove/MoveComponentSystem.hpp"
#include"Application/Components/Invincible/InvincibleSystem.hpp"
#include"Application/Components/Skill/System/SkillSystem.hpp"
#include"Application/Components/Enemy/AI/System/EnemyAISystem.hpp"
#include"Application/Components/Player/PlayerState/PlayerStateComponent.hpp"
#include"Application/Components/Status/StatusComponent.hpp"
#include"Application/Components/Tag/TagComponent.hpp"

#include"Application/Combat/CombatSystem.hpp"
#include"Application/Combat/CombatComponents.hpp"
#include"Application/Combat/CombatEvents.hpp"
#include"Application/Combat/GameFeel.hpp"
#include"Application/Objects/CreateObjects.hpp"
#include"Application/Scene/StageSelect/StageSelectScene.hpp"

namespace
{
	using namespace Engine;
	using Combat::CombatEvents;
	using Combat::CombatEventType;

	constexpr float AutoPlayStepTimeout = 6.0f;	//	自動テストでは時間で次へ進める
}

namespace Engine::Scene
{
	bool Tutorial::Initialize()
	{
		Engine::System::Camera* camera = new Engine::System::Camera();
		camera->Create();

		Combat::GameFeel::Reset();
		UI::Hud::SetEnabled(true);

		auto systems = System::SystemManager::GetInstance();
		systems->AddSystem<System::InputRequestSystem>();
		systems->AddSystem<System::CombatSystem>();
		systems->AddSystem<System::PlayerStateSystem>();
		systems->AddSystem<System::EnemyAISystem>();
		systems->AddSystem<System::MoveComponentSystem>();
		systems->AddSystem<System::DamageSystem>();
		systems->AddSystem<System::SocketComponentSystem>();
		systems->AddSystem<System::CameraControlSystem>();
		systems->AddSystem<System::InvincibleSystem>();
		systems->AddSystem<System::SkillSystem>();

		System::ColliderSystem::Initialize();
		System::ColliderSystem::AddCollisionPair<System::PlayerTag, System::EnemyTag>({ true,false,false,false });
		System::ColliderSystem::AddCollisionPair<System::PlayerWeaponTag, System::EnemyTag>({ false,false });
		System::ColliderSystem::AddCollisionPair<System::EnemyWeaponTag, System::PlayerTag>({ false,false });

		System::ObjectsFactory::CreateField();
		System::ObjectsFactory::CreatePlayer();
		mDummy = System::ObjectsFactory::CreateTrainingDummy();

		auto skyBoxResource = Graphics::SkyBoxResourceManager::GetInstance()->Load("Assets/SkyBox/skybox.dds");
		mSkyBox = std::make_unique<Graphics::SkyBox>();
		mSkyBox->Create(skyBoxResource);

		Audio::AudioManager::GetInstance()->PlaySE("Assets/StageSelect/Sound/BGM.aud", true, 0.12f, false);

		BuildSteps();
		EnterStep(0);
		return true;
	}

	void Tutorial::BuildSteps()
	{
		auto& reg = System::EntityManager::GetInstance()->GetRegistry();
		const entt::entity dummy = mDummy;

		auto setPassive = [&reg, dummy](bool Passive)
			{
				if (reg.valid(dummy) == false)
				{
					return;
				}
				if (Passive)
				{
					reg.emplace_or_replace<System::EnemyPassiveTag>(dummy);
				}
				else if (reg.all_of<System::EnemyPassiveTag>(dummy))
				{
					reg.remove<System::EnemyPassiveTag>(dummy);
				}
			};

		mSteps = {
			{ "移動", "まずは歩いてみよう。カメラの向きが前になる。", "WASD / 左スティック", 2,
				[setPassive]() { setPassive(true); } },
			{ "カメラ", "見回して周りを確認しよう。", "マウス / 右スティック", 1, nullptr },
			{ "攻撃", "木人に近づいて斬りつけよう。連続で押すとコンボになる。", "左クリック / R2", 3, nullptr },
			{ "回避", "回避中は攻撃を受けない。スタミナを使うので連発に注意。", "Shift / A", 2, nullptr },
			{ "ガード", "木人が攻撃してくる。防御ボタン長押しで受け止めよう。", "右クリック長押し / L2", 1,
				[setPassive]() { setPassive(false); } },
			{ "パリィ", "武器が光り「!」が出たら、当たる直前に防御ボタンを押す。", "右クリック / L2（タイミングよく）", 2, nullptr },
			{ "体幹崩し", "パリィと攻撃で体幹ゲージを満タンにすると、敵が崩れる。", "パリィ + 攻撃", 1, nullptr },
			{ "スキル", "Q と E のスキルを1回ずつ使おう。", "Q / E（L1 / R1）", 2, nullptr },
			{ "卒業試験", "木人を倒そう。崩れている間は大ダメージ！", "全ての技を使って", 1,
				[&reg, dummy]()
				{
					if (reg.valid(dummy) && reg.all_of<System::DummyInvulnerableTag>(dummy))
					{
						reg.remove<System::DummyInvulnerableTag>(dummy);
					}
				} },
		};
	}

	void Tutorial::EnterStep(int Index)
	{
		mCurrent = Index;
		mProgress = 0;
		mStepTime = 0.0f;
		mAccumulator = 0.0f;
		if (Index >= static_cast<int>(mSteps.size()))
		{
			return;
		}
		const Step& step = mSteps[Index];
		if (step.OnEnter)
		{
			step.OnEnter();
		}
		mObjective.Title = step.Title;
		mObjective.Body = step.Body;
		mObjective.KeyHint = step.KeyHint;
		mObjective.Step = Index + 1;
		mObjective.StepCount = static_cast<int>(mSteps.size());
		mObjective.Progress = 0;
		mObjective.ProgressMax = step.Goal;
		UI::Hud::SetObjective(&mObjective);
	}

	/// <summary>
	/// 今の課題の進み具合
	/// </summary>
	int Tutorial::CountProgress(float DeltaTime)
	{
		auto& reg = System::EntityManager::GetInstance()->GetRegistry();
		auto input = Input::InputManager::GetInstance();

		auto players = reg.view<System::PlayerStateComponent, System::PlayerTag>();
		const entt::entity playerEntity = players.front();
		const System::PlayerStateComponent* state = playerEntity == entt::null ? nullptr : &players.get<System::PlayerStateComponent>(playerEntity);

		switch (mCurrent)
		{
		case 0:	//	移動: 走っている時間(秒)
			if (state != nullptr && state->State == System::ePlayerState::Run)
			{
				mAccumulator += DeltaTime;
			}
			return static_cast<int>(mAccumulator);
		case 1:	//	カメラ
			if (input->GetLookAxis().SqrLength() > 0.01f)
			{
				mAccumulator += DeltaTime;
			}
			return mAccumulator >= 1.0f ? 1 : 0;
		case 2:
			return mProgress + CombatEvents::Count(CombatEventType::PlayerAttackHit);
		case 3:
			return mProgress + CombatEvents::Count(CombatEventType::Dodge);
		case 4:
			return mProgress + CombatEvents::Count(CombatEventType::Guard) + CombatEvents::Count(CombatEventType::Parry);
		case 5:
			return mProgress + CombatEvents::Count(CombatEventType::Parry);
		case 6:
			return mProgress + CombatEvents::Count(CombatEventType::PostureBreak);
		case 7:
			for (const auto& e : CombatEvents::Get())
			{
				if (e.Type == CombatEventType::SkillUsed && e.Extra >= 0 && e.Extra < 2)
				{
					mUsedSkill[e.Extra] = true;
				}
			}
			return (mUsedSkill[0] ? 1 : 0) + (mUsedSkill[1] ? 1 : 0);
		case 8:
			return mProgress + CombatEvents::Count(CombatEventType::EnemyKilled);
		default:
			return 0;
		}
	}

	void Tutorial::PostUpdate(double DeltaTime)
	{
		const float dt = static_cast<float>(DeltaTime);
		mObjective.CompleteFlash = (std::max)(0.0f, mObjective.CompleteFlash - dt * 1.5f);

		//	練習なので倒されない
		auto& reg = System::EntityManager::GetInstance()->GetRegistry();
		auto players = reg.view<System::StatusComponet, System::PlayerTag>();
		players.each([](System::StatusComponet& status)
			{
				if (status.GetHpRatio() < 0.3f)
				{
					status.Hp = status.MaxHp.GetCurrent();
				}
			});

		if (mFinishTimer >= 0.0f)
		{
			mFinishTimer += dt;
			if (mFinishTimer > 3.0f)
			{
				mFinishTimer = -100.0f;
				System::SceneManager::GetInstance()->ChangeSceneFade<Scene::StageSelect>();
			}
			return;
		}
		if (mCurrent >= static_cast<int>(mSteps.size()))
		{
			return;
		}

		mStepTime += dt;
		mProgress = CountProgress(dt);
		const bool autoAdvance = Core::LaunchOptions::Get().AutoPlay && mStepTime > AutoPlayStepTimeout;
		mObjective.Progress = mProgress;

		if (mProgress >= mSteps[mCurrent].Goal || autoAdvance)
		{
			mObjective.CompleteFlash = 1.0f;
			Audio::AudioManager::GetInstance()->PlaySE("Assets/Sound/Select.aud", false, 0.8f);
			if (mCurrent + 1 >= static_cast<int>(mSteps.size()))
			{
				mObjective.Title = "チュートリアル完了！";
				mObjective.Body = "ステージ選択へ戻ります。";
				mObjective.KeyHint.clear();
				mObjective.ProgressMax = 0;
				mFinishTimer = 0.0f;
				return;
			}
			EnterStep(mCurrent + 1);
		}
	}

	void Tutorial::Render()
	{
		mSkyBox->Render();
	}

	void Tutorial::Release()
	{
		UI::Hud::SetEnabled(false);
		System::EntityManager::GetInstance()->ClearLocalEntities();
	}
}
