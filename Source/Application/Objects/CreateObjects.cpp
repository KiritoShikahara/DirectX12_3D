#include "pch.h"
#include "CreateObjects.hpp"

#include"System/Entity/Manager/EntityManager.hpp"
#include"Graphics/FbxMesh/Resource/Manager/FbxResourceManager.hpp"
#include"Graphics/Texture/Manager/TextureManager.hpp"
#include"Graphics/Particles/ParticleSystem.hpp"

#include"System/Conponent/Rigidbody/RigidbodyComponent.hpp"
#include"System/Conponent/Transform/TransformConponent.hpp"
#include"System/Conponent/Fbx/FbxMeshConponent.hpp"
#include"System/Conponent/Collider/ColliderComponent.hpp"

#include"Application/Components/InputMove/MoveComponent.hpp"
#include"Application/Components/CameraWork/CameraControl.hpp"
#include"Application/Components/Socket/SocketComponent.hpp"
#include"Application/Components/Status/StatusComponent.hpp"
#include"Application/Components/Damage/AttackPowerComponent.hpp"

#include"Application/Components/Player/PlayerState/PlayerStateComponent.hpp"
#include"Application/Components/Player/Input/InputRequestComponent.hpp"

#include "Application/Components/Enemy/State/EnemyStateComponent.hpp"
#include "Application/Components/Enemy/AI/EnemyAIComponent.hpp"
#include "Application/Components/Enemy/Parameter/EnemyParametersComponent.hpp"

#include"Application/Components/Tag/TagComponent.hpp"
#include"Application/Components/StageComponent/StateComponent.hpp"
#include"Application/Components/Skill/SkillComponent.hpp"
#include"Application/Combat/CombatComponents.hpp"

#include"System/CSV/CSVManager.hpp"
#include"Application/Data/EnemyData.hpp"
#include"Application/Data/PlayerData.hpp"
#include"Application/Data/SkillData.hpp"

namespace
{
	using namespace Engine;
	using namespace Engine::System;

	constexpr float CharacterScale = 0.05f;

	/// <summary>
	/// キャラクター(プレイヤー・敵共通)のモデルとアニメーション
	/// </summary>
	Graphics::FbxResource* LoadCharacterResource()
	{
		auto res = Graphics::FbxResourceManager::GetInstance()->Load("Assets/Fbx/Ganfaul/Faul.fbx.bin");
		res->LoadAnimation("Idle", "Assets/Fbx/Ganfaul/Animation/Idle.fbx.anm");
		res->LoadAnimation("Jog", "Assets/Fbx/Ganfaul/Animation/Jog.fbx.anm");
		res->LoadAnimation("Dodge", "Assets/Fbx/Ganfaul/Animation/Dodge.fbx.anm");
		res->LoadAnimation("Attack_0", "Assets/Fbx/Ganfaul/Animation/Attack_A.fbx.anm");
		res->LoadAnimation("Attack_1", "Assets/Fbx/Ganfaul/Animation/Attack_B.fbx.anm");
		res->LoadAnimation("Attack_2", "Assets/Fbx/Ganfaul/Animation/Attack_C.fbx.anm");
		res->LoadAnimation("Attack_3", "Assets/Fbx/Ganfaul/Animation/Attack_D.fbx.anm");
		return res;
	}

	/// <summary>
	/// 体の当たり判定(頭のボーン基準の箱)
	/// </summary>
	void AddBodyCollider(entt::registry& Reg, entt::entity Entity)
	{
		auto col = ColliderComponent::Create<AABBCollider>();
		auto collider = col.GetPtr<AABBCollider>();
		collider->SetVolume({ 2.0f,8.0f,2.0f });
		col.Offset = { 0.0f, collider->GetVolume().y * 0.5f, 0.0f };
		col.BoneName = "Head";
		Reg.emplace<ColliderComponent>(Entity, std::move(col));
	}

	/// <summary>
	/// 剣(持ち手の位置合わせ込み)
	/// </summary>
	entt::entity CreateSword(entt::entity Parent, const std::string& BoneName, int Damage, const Graphics::Color& Tint)
	{
		auto& registry = EntityManager::GetInstance()->GetRegistry();
		auto sword = EntityManager::GetInstance()->CreateEntity();

		auto& transform = registry.emplace<Transform3D>(sword);
		transform.Scale = { 1.0f, 1.0f, 1.0f };
		transform.Rotation = Math::Quaternion::Identity;

		auto res = Graphics::FbxResourceManager::GetInstance()->Load("Assets/Sword/Sword.fbx.bin");
		auto& fbx = registry.emplace<FbxComponent>(sword, res, false);
		fbx.CurrAnimation = "";
		fbx.Mesh->SetColor(Tint);
		fbx.Mesh->SetMaterial(0.25f, 1.0f, 0.2f);

		auto& socket = registry.emplace<SocketComponent>(sword);
		socket.Parent = Parent;
		socket.BoneName = BoneName;
		socket.OffsetPos = { 1.82f, 0.51f, -0.46f };
		socket.OffsetRot = { -0.7f, -0.685f, 0.192f, 0.007f };
		socket.PivotOffset = { 0.0f, 1.98f, 0.0f };

		auto& damage = registry.emplace<AttackPowerComponent>(sword);
		damage.DamageValue = static_cast<float>(Damage);

		return sword;
	}

	void AddWeaponTrail(entt::registry& Reg, entt::entity Weapon, entt::entity Owner, const Graphics::Color& TrailColor)
	{
		auto& trail = Reg.emplace<WeaponTrailComponent>(Weapon);
		trail.Owner = Owner;
		trail.Trail = Graphics::ParticleSystem::GetInstance()->CreateTrail(TrailColor, 0.1f);
	}
}

entt::entity Engine::System::ObjectsFactory::CreatePlayer()
{
	auto manager = EntityManager::GetInstance();
	auto& registry = manager->GetRegistry();
	auto player = manager->CreateEntity();
	auto data = System::CSV::Get<PlayerData>().Find(1);

	auto& transform = registry.emplace<Transform3D>(player);
	transform.Position = { 0.0f,0.0f,0.0f };
	transform.Scale = { CharacterScale, CharacterScale, CharacterScale };
	transform.Rotation = Math::Quaternion::Identity;

	auto& fbx = registry.emplace<FbxComponent>(player, LoadCharacterResource());
	fbx.CurrAnimation = "Idle";
	fbx.Mesh->SetMaterial(0.55f, 0.25f, 0.5f);

	AddBodyCollider(registry, player);

	auto& rigidbody = registry.emplace<Rigidbody3D>(player);
	rigidbody.Flags |= RigidbodyFlags::UseCameraBasis;

	//	カメラ追従
	auto& camera = registry.emplace<System::CameraControlComponent>(player);
	camera.TargetEntity = player;
	camera.offset = { 0,8,0 };

	//	ステータス
	auto& status = registry.emplace<System::StatusComponet>(player);
	status.MaxHp.Base = data->MaxHP;
	status.Hp = data->MaxHP;
	registry.emplace<StaminaComponent>(player);
	registry.emplace<DefenseComponent>(player);

	registry.emplace<System::InputRequestComponent>(player);
	auto& state = registry.emplace<System::PlayerStateComponent>(player);
	state.Dodge.DodgeCountMax = data->DodgeMax;
	state.Dodge.CoolDowmMax = data->DodgeCT;
	state.Dodge.MoveSpeed = data->DodgeSpeed;

	auto& move = registry.emplace<System::MoveComponent>(player);
	move.MoveSpeed = data->MoveSpeed;
	move.TurnSpeed = data->TurnSpeed;

	//	武器
	auto sword = CreatePlayerWeapon(player,"RightHand", data->Attack);
	state.Weapon = sword;

	registry.emplace<PlayerTag>(player);

	//	スキル
	auto& skillHolder = registry.emplace<SkillSlotComponent>(player);
	skillHolder.SkillSlots[0] = CreateSkill1();
	skillHolder.SkillSlots[1] = CreateSkill2();

	return player;
}

entt::entity Engine::System::ObjectsFactory::CreateEnemy(const EnemySpec& Spec)
{
	auto manager = EntityManager::GetInstance();
	auto& registry = manager->GetRegistry();
	auto enemy = manager->CreateEntity();
	auto data = System::CSV::Get<EnemyData>().Find(Spec.DataId);

	auto& transform = registry.emplace<Transform3D>(enemy);
	transform.Position = Spec.SpawnPosition;
	transform.Scale = { CharacterScale, CharacterScale, CharacterScale };
	transform.Rotation = Math::Quaternion::Identity;

	registry.emplace<Rigidbody3D>(enemy);

	auto& fbx = registry.emplace<FbxComponent>(enemy, LoadCharacterResource());
	fbx.CurrAnimation = "Idle";
	//	体は色を控えめに(武器と発光で色を見せる)
	const float tintStrength = 0.65f;
	fbx.Mesh->SetColor({
		1.0f + (Spec.Tint.r - 1.0f) * tintStrength,
		1.0f + (Spec.Tint.g - 1.0f) * tintStrength,
		1.0f + (Spec.Tint.b - 1.0f) * tintStrength, 1.0f });
	fbx.Mesh->SetMaterial(0.45f, Spec.Metallic, 0.35f);

	auto& status = registry.emplace<System::StatusComponet>(enemy);
	status.MaxHp.Base = data->MaxHP;
	status.Hp = data->MaxHP;

	AddBodyCollider(registry, enemy);

	registry.emplace<EnemyStateComponent>(enemy);
	registry.emplace<EnemyAIComponent>(enemy);

	auto& param = registry.emplace<EnemyParameters>(enemy);
	param.Rank = Spec.Rank;
	param.MoveSpeed = data->MoveSpeed;
	param.EvadeSpeed = param.EvadeMaxSpeed = data->EvadeSpeed;
	param.AttackRange = data->AttackRange;
	param.IdleTime = data->IdleTime;
	param.IdleEvadeProbability = data->IdleEvadeProbability;
	param.CancelEvadeProbability = data->CancelEvadeProbability;
	param.AttackComboMax = data->AttackComboMax;
	param.CanCancelEvade = (bool)data->CanCancelEvade;
	param.AttackAnimScale = Spec.AttackAnimScale;

	//	戦闘(体幹・攻撃フェーズ)
	auto& posture = registry.emplace<PostureComponent>(enemy);
	posture.Value.Max = Spec.PostureMax;
	auto& combat = registry.emplace<EnemyCombatComponent>(enemy);
	combat.DisplayName = Spec.DisplayName;
	combat.WeaponColor = Spec.Tint;
	combat.UnparryableChance = Spec.UnparryableChance;
	combat.IsTrainingDummy = Spec.TrainingDummy;

	//	武器
	param.Weapon = CreateEnemyWeapon(enemy, "RightHand", data->Attack, Spec.Tint);

	registry.emplace<EnemyTag>(enemy);
	switch (Spec.Rank)
	{
	case EnemyRank::Basic: registry.emplace<BasicTag>(enemy); break;
	case EnemyRank::Advanced: registry.emplace<AdvancedTag>(enemy); break;
	case EnemyRank::Boss: registry.emplace<BossTag>(enemy); break;
	}
	return enemy;
}

/// <summary>
/// 青: 最初の敵。攻撃は全て弾ける
/// </summary>
void Engine::System::ObjectsFactory::CreateEnemy_Basic()
{
	EnemySpec spec;
	spec.DataId = 1;
	spec.Rank = EnemyRank::Basic;
	spec.DisplayName = "蒼刃の騎士";
	spec.Tint = Graphics::Color::Cyan();
	spec.AttackAnimScale = 0.65f;
	spec.PostureMax = 90.0f;
	CreateEnemy(spec);
}

/// <summary>
/// 黄: 中盤。攻撃が速い
/// </summary>
void Engine::System::ObjectsFactory::CreateEnemy_Advanced()
{
	EnemySpec spec;
	spec.DataId = 2;
	spec.Rank = EnemyRank::Advanced;
	spec.DisplayName = "金獅子の剣士";
	spec.Tint = Graphics::Color::Yellow();
	spec.AttackAnimScale = 0.8f;
	spec.UnparryableChance = 0.1f;
	spec.PostureMax = 110.0f;
	spec.Metallic = 0.85f;
	CreateEnemy(spec);
}

/// <summary>
/// 赤: ボス。弾けない攻撃(赤い閃光)が混ざる
/// </summary>
void Engine::System::ObjectsFactory::CreateEnemy_Boss()
{
	EnemySpec spec;
	spec.DataId = 3;
	spec.Rank = EnemyRank::Boss;
	spec.DisplayName = "紅蓮の剣鬼";
	spec.Tint = Graphics::Color::Red();
	spec.AttackAnimScale = 0.9f;
	spec.UnparryableChance = 0.25f;
	spec.PostureMax = 140.0f;
	spec.Metallic = 0.7f;
	CreateEnemy(spec);
}

/// <summary>
/// チュートリアルの木人: 遅く、弾ける攻撃だけをする
/// </summary>
entt::entity Engine::System::ObjectsFactory::CreateTrainingDummy()
{
	EnemySpec spec;
	spec.DataId = 1;
	spec.Rank = EnemyRank::Basic;
	spec.DisplayName = "修練の木人";
	spec.Tint = { 0.85f, 0.7f, 0.5f, 1.0f };
	spec.SpawnPosition = { 0.0f, 0.0f, 16.0f };
	spec.AttackAnimScale = 0.45f;
	spec.PostureMax = 100.0f;
	spec.Metallic = 0.1f;
	spec.TrainingDummy = true;
	auto enemy = CreateEnemy(spec);

	auto& registry = EntityManager::GetInstance()->GetRegistry();
	auto& param = registry.get<EnemyParameters>(enemy);
	param.MoveSpeed *= 0.5f;
	param.IdleTime = 1.8f;
	param.IdleEvadeProbability = 0.0f;
	param.CancelEvadeProbability = 0.0f;
	param.CanCancelEvade = false;
	param.AttackComboMax = 1;
	registry.emplace<DummyInvulnerableTag>(enemy);

	//	練習中に体力が尽きないように多め
	auto& status = registry.get<StatusComponet>(enemy);
	status.MaxHp.Base *= 4.0f;
	status.Hp = status.MaxHp.Base;
	return enemy;
}

void Engine::System::ObjectsFactory::CreateField()
{
	auto manager = EntityManager::GetInstance();
	auto& registry = manager->GetRegistry();
	auto field = manager->CreateEntity();

	const float volume = 1.8f;
	auto& trans = registry.emplace<Transform3D>(field);
	trans.Position = { 0.0f,0.0f,0.0f };
	trans.Scale = { volume,volume,volume };

	auto res = Graphics::FbxResourceManager::GetInstance()->Load("Assets/Fbx/Field/Field.fbx.bin");
	auto& fbx = registry.emplace<FbxComponent>(field, res,false);

	//	月面のPBRテクスチャ(Poly Haven moon_02 / CC0)をワールド座標でタイリング
	auto textures = Graphics::TextureManager::GetInstance();
	fbx.Mesh->SetSurface(
		textures->Load("Assets/Texture/Terrain/moon_02_diff_2k.jpg"),
		textures->Load("Assets/Texture/Terrain/moon_02_nor_dx_2k.jpg"),
		textures->Load("Assets/Texture/Terrain/moon_02_arm_2k.jpg"),
		0.16f);
	fbx.Mesh->SetMaterial(0.9f, 0.0f, 0.0f);
	fbx.CastShadow = false;

	auto& stageConfig = registry.emplace<StageComponent>(field);
	stageConfig.PlayableRadius = 80.f;

	registry.emplace<StageTag>(field);
}

entt::entity Engine::System::ObjectsFactory::CreatePlayerWeapon(entt::entity Parent, const std::string& BoneName, int Damage)
{
	auto& registry = EntityManager::GetInstance()->GetRegistry();
	auto sword = CreateSword(Parent, BoneName, Damage, Graphics::Color::White());
	registry.get<FbxComponent>(sword).Mesh->SetEmissive({ 0.4f, 0.65f, 1.0f, 1.0f }, 0.15f);
	AddWeaponTrail(registry, sword, Parent, { 0.35f, 0.65f, 1.3f, 0.7f });
	registry.emplace<PlayerWeaponTag>(sword);
	return sword;
}

entt::entity Engine::System::ObjectsFactory::CreateEnemyWeapon(entt::entity Parent, const std::string& BoneName, int Damage, const Graphics::Color& Color)
{
	auto& registry = EntityManager::GetInstance()->GetRegistry();
	auto sword = CreateSword(Parent, BoneName, Damage, Color);
	AddWeaponTrail(registry, sword, Parent, { Color.r * 1.2f, Color.g * 1.2f, Color.b * 1.2f, 0.6f });
	registry.emplace<EnemyWeaponTag>(sword);
	return sword;
}

/// <summary>
/// スキル1: 風(周囲を斬り裂く竜巻)
/// </summary>
entt::entity Engine::System::ObjectsFactory::CreateSkill1()
{
	auto manager = EntityManager::GetInstance();
	auto& registry = manager->GetRegistry();
	auto entity = manager->CreateEntity();
	auto data = System::CSV::Get<SkillData>().Find(1);

	auto& skill = registry.emplace<SkillComponent>(entity);
	skill.Kind = SkillComponent::eKind::Wind;
	skill.Radius = 10.0f;
	skill.DisplayName = "疾風";

	registry.emplace<Transform3D>(entity);
	auto& damage = registry.emplace<AttackPowerComponent>(entity);
	damage.DamageValue = data->Attack;
	registry.emplace<PlayerWeaponTag>(entity);
	return entity;
}

/// <summary>
/// スキル2: 炎(火柱)
/// </summary>
entt::entity Engine::System::ObjectsFactory::CreateSkill2()
{
	auto manager = EntityManager::GetInstance();
	auto& registry = manager->GetRegistry();
	auto entity = manager->CreateEntity();
	auto data = System::CSV::Get<SkillData>().Find(2);

	auto& skill = registry.emplace<SkillComponent>(entity);
	skill.MaxCooldown = 4.0f;
	skill.Kind = SkillComponent::eKind::Fire;
	skill.Radius = 8.0f;
	skill.DisplayName = "紅炎";

	registry.emplace<Transform3D>(entity);
	auto& damage = registry.emplace<AttackPowerComponent>(entity);
	damage.DamageValue = data->Attack;
	registry.emplace<PlayerWeaponTag>(entity);
	return entity;
}
