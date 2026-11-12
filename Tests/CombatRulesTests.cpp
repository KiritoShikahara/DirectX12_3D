// 戦闘ルールの単体テスト (Tools/build_test.sh から cl.exe でビルド・実行)
#include "Application/Combat/CombatRules.hpp"
#include <cmath>
#include <cstdio>

using namespace Engine::Combat;

static int gFailed = 0;
static int gPassed = 0;

#define CHECK(expr) do { if (expr) { ++gPassed; } else { ++gFailed; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } } while (0)
#define CHECK_NEAR(a, b) CHECK(std::fabs((a) - (b)) < 1e-4f)

static void TestParryWindow()
{
	DefenseInput input;
	input.Holding = true;
	input.TimeSincePress = 0.0f;
	CHECK(ResolveDefense(input, true, 100.0f) == DefenseResult::Parried);

	input.TimeSincePress = Tuning::ParryWindow;
	CHECK(ResolveDefense(input, true, 100.0f) == DefenseResult::Parried);

	input.TimeSincePress = Tuning::ParryWindow + 0.01f;
	CHECK(ResolveDefense(input, true, 100.0f) == DefenseResult::Guarded);

	//	弾けない攻撃はジャストでもガード扱い
	input.TimeSincePress = 0.0f;
	CHECK(ResolveDefense(input, false, 100.0f) == DefenseResult::Guarded);

	//	連打ロック中はパリィにならない
	input.ParryLocked = true;
	CHECK(ResolveDefense(input, true, 100.0f) == DefenseResult::Guarded);
}

static void TestGuardAndHit()
{
	DefenseInput input;
	CHECK(ResolveDefense(input, true, 100.0f) == DefenseResult::Hit);

	input.Holding = true;
	input.TimeSincePress = 1.0f;
	CHECK(ResolveDefense(input, true, Tuning::GuardStaminaCost) == DefenseResult::Guarded);
	CHECK(ResolveDefense(input, true, Tuning::GuardStaminaCost - 0.1f) == DefenseResult::GuardBroken);

	CHECK_NEAR(DamageAfterDefense(DefenseResult::Parried, 40.0f), 0.0f);
	CHECK_NEAR(DamageAfterDefense(DefenseResult::Guarded, 40.0f), 40.0f * Tuning::GuardDamageRate);
	CHECK_NEAR(DamageAfterDefense(DefenseResult::GuardBroken, 40.0f), 20.0f);
	CHECK_NEAR(DamageAfterDefense(DefenseResult::Hit, 40.0f), 40.0f);
}

static void TestPosture()
{
	Posture p;
	CHECK(AddPosture(p, Tuning::ParryPostureDamage) == false);
	CHECK(AddPosture(p, Tuning::ParryPostureDamage) == false);
	CHECK(AddPosture(p, Tuning::ParryPostureDamage) == true);	//	3回パリィで崩れる
	CHECK(p.IsBroken());
	CHECK_NEAR(DamageMultiplier(p), Tuning::BreakDamageRate);

	//	崩れ中は加算しても再度崩れない
	CHECK(AddPosture(p, 50.0f) == false);

	//	崩れ時間が終わると空に戻る
	UpdatePosture(p, Tuning::BreakDuration + 0.01f);
	CHECK(p.IsBroken() == false);
	CHECK_NEAR(p.Value, 0.0f);
	CHECK_NEAR(DamageMultiplier(p), 1.0f);

	//	攻撃を受けない時間が続くと回復
	Posture q;
	AddPosture(q, 50.0f);
	UpdatePosture(q, Tuning::PostureRecoverDelay - 0.1f);
	CHECK_NEAR(q.Value, 50.0f);
	UpdatePosture(q, 0.1f);
	UpdatePosture(q, 1.0f);
	CHECK_NEAR(q.Value, 50.0f - Tuning::PostureRecoverPerSecond * 1.1f);	//	遅延到達後の 0.1 + 1.0 秒分
}

static void TestStamina()
{
	Stamina s;
	CHECK(TryConsumeStamina(s, Tuning::DodgeStaminaCost));
	CHECK_NEAR(s.Value, Tuning::StaminaMax - Tuning::DodgeStaminaCost);

	//	消費直後は回復しない
	UpdateStamina(s, Tuning::StaminaRegenDelay * 0.5f);
	CHECK_NEAR(s.Value, Tuning::StaminaMax - Tuning::DodgeStaminaCost);

	//	遅延の後に回復し、最大で止まる
	UpdateStamina(s, Tuning::StaminaRegenDelay);
	UpdateStamina(s, 10.0f);
	CHECK_NEAR(s.Value, Tuning::StaminaMax);

	s.Value = 10.0f;
	CHECK(TryConsumeStamina(s, Tuning::DodgeStaminaCost) == false);
	CHECK_NEAR(s.Value, 10.0f);
	ForceConsumeStamina(s, 50.0f);
	CHECK_NEAR(s.Value, 0.0f);
}

static void TestAttackPhase()
{
	CHECK(GetAttackPhase(0.0f) == AttackPhase::Windup);
	CHECK(GetAttackPhase(Tuning::TelegraphAt) == AttackPhase::Windup);
	CHECK(GetAttackPhase(Tuning::ActiveBegin) == AttackPhase::Active);
	CHECK(GetAttackPhase(Tuning::ActiveEnd - 0.01f) == AttackPhase::Active);
	CHECK(GetAttackPhase(Tuning::ActiveEnd) == AttackPhase::Recovery);
	CHECK(GetAttackPhase(1.0f) == AttackPhase::Recovery);
	//	予告は攻撃判定より前
	CHECK(Tuning::TelegraphAt < Tuning::ActiveBegin);
}

int main()
{
	TestParryWindow();
	TestGuardAndHit();
	TestPosture();
	TestStamina();
	TestAttackPhase();
	std::printf("CombatRulesTests: %d passed, %d failed\n", gPassed, gFailed);
	return gFailed == 0 ? 0 : 1;
}
